/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

// #define _XOPEN_SOURCE	/* glibc2 needs this */

#include <ctime>

#include <nidas/core/FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/core/Project.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/Variable.h>
#include <nidas/core/NidasApp.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Process.h>
#include <nidas/util/Logger.h>
#include <nidas/util/auto_ptr.h>
#include <nidas/util/util.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>
#include <sys/stat.h>

#include <unistd.h>

#include <stdexcept>

#include <fstream> //for writing processed data to a .txt file
#include <string>  //for converting data to_string
#include <sstream>

#include <stdio.h> // for making an api request to insert data into influxdb
#include <curl/curl.h>
#include <curl/easy.h>
#include <future> //async function calls
#include <mutex> //locking the string of data to reset to empty, may not be useful?

#include <json/json.h>


using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

/**
 * Simple char buffer class to provide memory into which C strings can be
 * written.  Memory grows as necessary and is not reclaimed, and the length
 * of the buffer is tracked so strings can be written right at the end
 * instead of appended to a long string.
 **/
class CharBuffer
{
public:
    CharBuffer() :
        _buffer(1),
        _buflen(0)
    {
        // The buffer always has at least one null byte in it, so it
        // looks like an empty null-terminated string.
        clear();
    }

    void
    clear()
    {
        _buffer[0] = '\0';
        _buflen = 0;
    }

    /**
     * Return a pointer to the beginning of the buffer.
     **/
    char*
    get()
    {
        return &(_buffer[0]);
    }

    /**
     * Get a pointer to the end of the buffer with room for at least @p
     * length more bytes, but do not change the length of space used in the
     * buffer yet.  The returned pointer can be used to print or copy up to
     * @p length bytes into the buffer, beginning at the pointer returned
     * by getSpace().  A null terminator is automatically added at the new
     * length, so the caller does not need to add a null terminator if it
     * writes exactly @p length bytes into the buffer.
     **/
    char*
    getSpace(unsigned int length = 0)
    {
        // Make sure _buflen is updated to include any strings written
        // since last called.
        _buflen += strlen(get() + _buflen);
        // Need room for the null byte too, since that is presumed not to
        // be included in length, but don't extend unless some space was
        // actually requested.
        if (length > 0)
        {
            _buffer.resize(_buflen + length + 1);
            _buffer[_buflen + length] = '\0';
        }
        return &(_buffer[_buflen]);
    }

    bool
    empty()
    {
        // Empty if the end of the buffer is at the beginning.  Use
        // getSpace() to find the end by updating buflen.
        return getSpace() == get();
    }

private:
    // Use a vector to manage memory for a char buffer.
    vector<char> _buffer;
    unsigned int _buflen;

    CharBuffer(const CharBuffer& rhs) :
        _buffer(rhs._buffer),
        _buflen(rhs._buflen)
    {
        VLOG(("CharBuffer copy constructed..."));
    }

    CharBuffer& operator=(const CharBuffer& rhs)
    {
        _buffer = rhs._buffer;
        _buflen = rhs._buflen;
        VLOG(("CharBuffer copy assignment..."));
        return *this;
    }

};



CURLcode
dataToInfluxDB(CURL* curl, const std::string& url,
               CharBuffer* data, unsigned int nmeasurements);

size_t
writeInfluxResult(void *buffer, size_t size, size_t nmemb, void *userp);

/**
 * Methods and memory for creating an Influx database, accumulating
 * measurements, and posting them to the database.
 *
 * The host URL is something like "http://snoopy.eol.ucar.edu:8086"
 *
 * The database URL is formed like "<url>/write?db=<dbname>&precision=u"
 **/
class InfluxDB
{
public:
    InfluxDB(const std::string& url = "",
             const std::string& dbname = "") :
        _url(url),
        _dbname(dbname),
        _data(0),
        _data1(),
        _data2(),
        _result(),
        _errs(),
        _nmeasurements(0),
        _total_measurements(0),
        _count(1),
        _echo(false),
        _async(true),
        _curl(0),
        _post()
    {
        _curl = curl_easy_init();
        if (! _curl)
        {
            throw std::runtime_error("curl_easy_init failed.");
        }
        // Tell libcurl to pass all data to the writeInfluxResult
        curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, writeInfluxResult);
        curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &_result);
        curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, _curl_errors);
        _data = &_data1;
    }

    void
    setURL(const std::string& url)
    {
        // Make sure the url has no trailing /, since influx does not parse
        // multiple slashes.  Instead it just responds with 404.
        _url = url;
        while (_url.length() > 0 && _url[_url.length() - 1] == '/')
        {
            _url.erase(_url.length() - 1);
        }
    }
        
    void
    setDatabase(const std::string& dbname)
    {
        _dbname = dbname;
    }

    void
    setUser(const std::string& username, const std::string& password)
    {
        _username = username;
        _password = password;

    }

    std::string
    getAuth(const std::string& prefix = "")
    {
        std::string parms;
        if (_username.length() || _password.length())
        {
            parms += prefix;
            parms += "u=" + _username + "&p=" + _password;
        }
        return parms;
    }

    void
    setCount(unsigned int count)
    {
        _count = count;
    }

    /**
     * If @p echo is true, the data posted to the database is printed on
     * stdout instead of being written to the database.
     **/
    void
    setEcho(bool echo)
    {
        _echo = echo;
    }

    /**
     * If @p enable is true, data will be posted to the database
     * asynchronously.
     **/
    void
    setAsync(bool enable)
    {
        _async = enable;
    }

    ~InfluxDB()
    {
        curl_easy_cleanup(_curl);
        _curl = 0;
    }

    std::string
    getHostURL()
    {
        return _url;
    }

    std::string
    getWriteURL()
    {
        ostringstream out;
        out << getHostURL() << "/write?db=" << _dbname << "&precision=u" ;
        out << getAuth("&");
        return out.str();
    }

    std::string
    getErrors()
    {
        return _errs;
    }

    /**
     * Return false if there is an error adding this measurement.  If
     * enough measurements have accumulated, then this will post the
     * current buffer to the database. If there is an error, the
     * description will be in getError().
     **/
    bool
    addMeasurement(const std::string& data)
    {
        char* buf = _data->getSpace(data.length());
        VLOG(("adding data to buffer..."));
        strcpy(buf, data.c_str());
        ++_nmeasurements;
        if (_nmeasurements >= _count)
        {
            return sendData();
        }
        return true;
    }

    bool
    sendData()
    {
        if (!_errs.empty())
        {
            // We're in an error state, meaning some previous attempt to
            // write data has failed, and no further attempts will be made,
            // so just clear the data.
            _data->clear();
            return false;
        }

        // If any error messages are accumulated in this stream, they are
        // set to the error string member.  We don't throw exceptions in
        // this method because it is likely called from a receive() method
        // which is not running in the main thread.
        std::ostringstream errs;
        
        DLOG(("sendData()...") << "async:" << _async);

        // Do one of three things with the current data: echo, send it
        // asynchronously, or send it synchronously.
        CURLcode res = CURLE_OK;
        if (_echo)
        {
            std::cout << _data->get();
            _data->clear();
        }
        else if (_async)
        {
            // Make sure any current _post future is finished before
            // launching another one on the next buffer.
            if (_post.valid())
            {
                res = _post.get();
            }
        }
        else if (!_data->empty())
        {
            res = dataToInfluxDB(_curl, getWriteURL(), _data, _nmeasurements);
        }
        // First see if the curl call itself failed.
        if (res != CURLE_OK)
        {
            errs << "http post failed: " << _curl_errors;
        }
        // Then see if the json result indicates an error.
        if (!_result.empty())
        {
            try {
                std::istringstream js(_result.get());
                Json::Value root;
                js >> root;
                Json::Value error = root["error"];
                string dnf = "database not found";
                if (!error.isNull() &&
                    error.asString().substr(0, dnf.size()) == dnf)
                {
                    errs << error.asString()
                         << "; maybe use --create to create it first?";
                }
                else if (!error.isNull())
                {
                    errs << error.asString();
                }
                else
                {
                    // Any result at all is probably an error.
                    errs << _result.get();
                }
            }
            catch (const Json::LogicError&)
            {
                errs << "Server response could not be parsed as json: ";
                errs << _result.get();
            }
        }

        // Done with the result, prepare it for another posting.
        _result.clear();
        _errs = errs.str();

        if (_async && _errs.empty() && !_data->empty())
        {
            _post = std::async(std::launch::async, &dataToInfluxDB,
                               _curl, getWriteURL(), _data, _nmeasurements);
            // Now swap the _data pointer to stop writing into the buffer
            // which has just been passed to the async call.  This only has
            // to happen if async enabled, since otherwise we can just keep
            // rewriting the same buffer.
            if (_data == &_data1)
                _data = &_data2;
            else
                _data = &_data1;
        }

        if (_errs.empty())
        {
            _total_measurements += _nmeasurements;
            _nmeasurements = 0;
        }
        return _errs.empty();
    }

    /**
     * Return the total number of measurements written to the database so
     * far.  This does not include the measurements currently in the buffer
     * and not yet written.
     **/
    unsigned int
    totalMeasurements()
    {
        return _total_measurements;
    }

    /**
     * Send whatever is left in the current buffer and wait for it to
     * finish.
     **/
    void
    flush()
    {
        ILOG(("flushing database writes..."));
        sendData();
        // In case the first sendData() call started an async posting,
        // call it again to make sure it finishes.
        if (!sendData())
        {
            throw n_u::Exception(getErrors());
        }
    }

    void
    createInfluxDB();

private:

    InfluxDB(const InfluxDB&);
    InfluxDB& operator=(const InfluxDB&);

    string _url;
    string _dbname;
    string _username;
    string _password;

    // _data points to the current data buffer to which data will be added.
    // This is a double-buffering scheme to allow one buffer to be written
    // while the other posts to influxdb.
    CharBuffer* _data;
    CharBuffer _data1;
    CharBuffer _data2;
    CharBuffer _result;

    // If an error is encountered, preserve the information here and then
    // skip any further writes to the database.
    string _errs;

    // This is always the number of measurements currently stored in the
    // active data buffer.
    unsigned int _nmeasurements;

    // The total number of measurements written to the database.
    unsigned int _total_measurements;

    // The maximum number of measurements to store in the buffer before
    // sending it to the database.
    unsigned int _count;

    bool _echo;

    // Use asynchronous calls to post data buffer to the database.
    bool _async;
    
    CURL *_curl;
    char _curl_errors[CURL_ERROR_SIZE];

    std::future<CURLcode> _post;
};


/**
 * Escape comma, space, and equal characters in the given string with
 * backslash, to conform to influxdb line protocol for tag values.
 **/
void
backslash(std::string& tagvalue)
{
    char targets[] = { ' ', '=', ',' };
    int ntargets = sizeof(targets)/sizeof(targets[0]);
    for (char* tc=targets; tc < targets+ntargets; ++tc)
    {
        size_t pos = 0;
        while (pos != string::npos && pos < tagvalue.length())
        {
            pos = tagvalue.find(*tc, pos);
            if (pos != string::npos)
            {
                tagvalue.insert(pos, 1, '\\');
                pos += 2;
            }
        }
    }
}



class SampleToDatabase
{
//   private: 
//     class dbSenderThread //inherit from boost lib
//         static threadPool;  //thread pool buffer
  
  public:
    /**
     * A default constructor is required to use objects as a map element.
     **/
    SampleToDatabase(InfluxDB* db = 0,
                     dsm_sample_id_t sid = 0, const std::string &sname = "",
                     const SampleTag *stag = 0) :
        _db(db),
        name(sname),
        id(sid),
        sitename(),
        measurementName(),
        spsid(),
        dsmid(),
        info(),
        varunits(),
        varnames()
    {
        if (!stag)
        {
            return;
        }
        if (!stag->getSite())
        {
            NidasApp* app = NidasApp::getApplicationInstance();
            WLOG(("sample tag has no site, cannot import: ")
                 << app->formatId(sid));
            return;
        }
        const std::vector<const Variable *> &variables = stag->getVariables();
        for (unsigned int i = 0; i < variables.size(); ++i)
        {
            varnames.push_back(variables[i]->getName());
            string units = variables[i]->getUnits();
            const VariableConverter* vc = variables[i]->getConverter();
            if (vc)
            {
                units = vc->getUnits();
            }
            backslash(units);
            varunits.push_back(units);
        }
        setSiteAndMeasurement(stag);

        dsmid = to_string(stag->getDSMId());
        int tempSpSid = stag->getSpSId();
        if (tempSpSid >= 0x8000)
        {
            stringstream intToHex;
            intToHex << "0x" << hex << tempSpSid;
            spsid = intToHex.str();
        }
        else
        {
            spsid = to_string(tempSpSid);
        }

        info = measurementName + ",dsm_id=" + dsmid + ",location=" + sitename;
        const DSMSensor* dsm = stag->getDSMSensor();
        if (dsm)
        {
            string height = dsm->getHeightString();
            if (height.length() > 0)
            {
                info += ",height=" + height;
            }
        }
        info += ",sps_id=" + spsid;
    }

    void
    setSiteAndMeasurement(const SampleTag* stag)
    {
        const Site* site = stag->getSite();

        // We have to distinguish between the EOL weather stations and all
        // other ISFS projects, since for the weather stations the site
        // names must come from the sensor suffix.  So use the fact that
        // only the weather stations will use a site name of eol-rt-data.
        sitename = site->getName();

        // EOL-WEATHER-STATIONS
        if (sitename == "eol-rt-data")
        {
            sitename = stag->getSuffix();
            sitename.erase(0,1);
        }
        measurementName = sitename;
    }

    bool
    receive(const Sample *samp) throw();

    void
    accumulate(const Sample *samp);

    InfluxDB* _db;

    string name;
    dsm_sample_id_t id;

    // Stash the site name from the sample tag to identify the
    // site in the accumulated data.
    string sitename;

    // To store the measurement name comprised of dsm id and sensor id concatenated
    // so that it is unique for each variable
    string measurementName;

    //gathering dsmid and spsid necessary to create measurement name
    string spsid;
    string dsmid;
    string info;

    // Stash the variable names from the sample tag to identify the
    // variables and units in the accumulated data.
    vector<string> varunits;
    vector<string> varnames;

public:
    SampleToDatabase& operator=(const SampleToDatabase& rhs)
    {
        if (this != &rhs)
        {
            _db = rhs._db;
            name = rhs.name;
            id = rhs.id;
            sitename = rhs.sitename;
            measurementName = rhs.measurementName;
            spsid = rhs.spsid;
            dsmid = rhs.dsmid;
            info = rhs.info;
            varunits = rhs.varunits;
            varnames = rhs.varnames;
        }
        return *this;
    }

    SampleToDatabase(const SampleToDatabase& rhs) :
        _db(),
        name(),
        id(),
        sitename(),
        measurementName(),
        spsid(),
        dsmid(),
        info(),
        varunits(),
        varnames()
    {
        *this = rhs;
    }
};


bool
SampleToDatabase::
receive(const Sample *samp) throw()
{
    if (samp->getType() != FLOAT_ST && samp->getType() != DOUBLE_ST)
    {
        return true;
    }
    if (! _db)
    {
        // There was no SampleTag for this sample, so we cannot send it to
        // the database.
        return true;
    }

    accumulate(samp);
    return true;
}


/*
 * This is invoked from main() at the initialization of the data to be
 * transferred to the influx database. This function creates an influx
 * database at the specfied url. createInfluxDB() uses the curl library for
 * assigning the url and the consequent fields to post during the HTTP POST
 * operation. Additionally, the string variable DBname can be assigned with
 * the DSMConfig information such as location or project name as this is
 * only created once.
 */
void
InfluxDB::
createInfluxDB()
{
    CURLcode res;

    string url = getHostURL() + "/query?" + getAuth();
    string createDB = "q=CREATE+DATABASE+" + _dbname;
    string full = url + "&" + createDB;

    ILOG(("creating database: ") << full);

    curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, createDB.c_str());
    res = curl_easy_perform(_curl);

    if (res != CURLE_OK)
    {
        string err("createInfluxDB() failed: ");
        err += full;
        throw std::runtime_error(err);
    }
    _result.clear();
}



size_t
writeInfluxResult(void *buffer, size_t size, size_t nmemb, void *userp)
{
    size_t length = size*nmemb;
    CharBuffer* result = static_cast<CharBuffer*>(userp);
    memcpy(result->getSpace(length), buffer, length);
    return length;
}


/**
 * Post the given data buffer to the database, then clear it.  This is a
 * function and not a method of InfluxDB, so that it only uses the
 * variables passed into it and no locking is needed to ensure exclusive
 * access to the object.  The CURL pointer is only ever used here, so as
 * long as only double-buffering is used, only one thread should ever call
 * into the curl library at a time.
 **/
CURLcode
dataToInfluxDB(CURL* curl, const std::string& url,
               CharBuffer* data, unsigned int nmeasurements)
{
    DLOG(("posting ") << nmeasurements << " measurements: " << url);
    DLOG(("data:") << data->get());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data->get());
    CURLcode res = curl_easy_perform(curl);
    data->clear();
    if (res != CURLE_OK)
    {
        ELOG(("database write failed: ") << res);
    }
    DLOG(("posting done."));
    return res;
}


void SampleToDatabase::
accumulate(const Sample *samp)
{
    if (samp->getType() != FLOAT_ST && samp->getType() != DOUBLE_ST)
    {
        return;
    }

    unsigned int nvalues = varnames.size();
    string data;
    string timeStamp = to_string(samp->getTimeTag());

    for (unsigned int i = 0; i < nvalues; ++i)
    {
        double value = samp->getDataValue(i);
        if (std::isnan(value))
        {
            // TO DO: should we have a boolean field set to true is the
            // value is an NaN? write to db? or just continue?
            continue;
        }
        else
        {
            data = info;
            if (varunits[i].length() > 0)
            {
                data += ",units=" + varunits[i];
            }
            data += " ";
            data += varnames[i];
            data += "=";
            data += to_string(value);
            data += " ";
            data += timeStamp;
            data += "\n";
            if (!_db->addMeasurement(data))
            {
                // An error occurred, so set an app exception to interrupt
                // the main loop.
                NidasApp* app = NidasApp::getApplicationInstance();
                if (app)
                {
                    app->setException(n_u::Exception(_db->getErrors()));
                }
            }
        }
    }
}


class SampleDispatcher : public SampleClient
{
public:
    SampleDispatcher(InfluxDB* db,
                     const list<DSMSensor *> &sensors, NidasApp &app);

    virtual ~SampleDispatcher() {}

    void flush() throw() {}

    bool receive(const Sample *samp) throw();

private:
    typedef map<dsm_sample_id_t, SampleToDatabase> sample_map_t;

    /**
     * Find the SampleToDatabase for the given sample ID.  Wisard samples get
     * mapped to one sensor type, so we look for all of them.
     **/
    sample_map_t::iterator
    findStats(dsm_sample_id_t sampid)
    {
        dsm_sample_id_t sid = sampid;
        sample_map_t::iterator it = _samples.end();
        if (sid & 0x8000)
        {
            sid = sid ^ (sid & 3);
            dsm_sample_id_t endid = sid + 4;
            VLOG(("searching from ")
                 << _app.formatId(sid) << " to " << _app.formatId(endid)
                 << " to match " << _app.formatId(sampid));
            while (sid < endid && it == _samples.end())
            {
                it = _samples.find(sid++);
            }
        }
        else
        {
            it = _samples.find(sid);
        }
        return it;
    }

    sample_map_t _samples;

    NidasApp &_app;
};



SampleDispatcher::SampleDispatcher(InfluxDB* db,
                                   const list<DSMSensor *> &sensors,
                                   NidasApp &app) :
    _samples(),
    _app(app)
{
    DLOG(("SampleDispatcher::SampleDispatcher()..."));
    SampleMatcher &matcher = _app.sampleMatcher();
    list<DSMSensor *>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si)
    {
        // Create a SampleToDatabase for processed samples from the given
        // sensors.
        DSMSensor *sensor = *si;
        SampleTagIterator ti = sensor->getSampleTagIterator();
        for (; ti.hasNext();)
        {
            const SampleTag *stag = ti.next();

            if (stag->getVariables().size() > 0)
            {
                string varname = stag->getVariables().front()->getName();
                if (stag->getVariables().size() > 1)
                {
                    varname += ",...";
                }
                // As a special case for wisard sensors, mask the last two
                // bits of the IDs so all "sensor types" aka I2C addresses
                // are treated like the same kind of sample.  We use the
                // first such ID and then map any others to that one, since
                // in most cases only one such ID will ever appear for all
                // four possible "sensor types".  However, there is some
                // risk this could hide multiple sensor types appearing in
                // the stream.  We can warn for that later if it happens.
                // Since the wisard ID mapping is taken care of in
                // findStats(), here we just add the sensor if the ID does
                // not already have a stats entry.
                //
                // Note this just adds the first of possibly multiple
                // "sensor types" assigned to a sample.  The actual sample
                // IDs are not known until samples are received.  So that's
                // the point at which we can correct the ID so it is
                // accurate in the reports.
                dsm_sample_id_t sid = stag->getId();
                if (!matcher.match(sid))
                {
                    continue;
                }
                sample_map_t::iterator it = findStats(sid);
                if (it == _samples.end())
                {
                    DLOG(("adding processed sample: ") << _app.formatId(sid));
                    SampleToDatabase pstats(db, sid, varname, stag);
                    _samples[pstats.id] = pstats;
                }
            }
        }
    }
}

bool SampleDispatcher::receive(const Sample *samp) throw()
{
    dsm_sample_id_t sampid = samp->getId();
    if (!_app.sampleMatcher().match(sampid))
    {
        return false;
    }
    VLOG(("received and accepted sample ") << _app.formatId(sampid));
    sample_map_t::iterator it = findStats(sampid);
    if (it == _samples.end())
    {
        WLOG(("received sample which is not in the config: ")
              << _app.formatId(sampid));
        // When there is no header from which to gather samples ahead of
        // time, just add a SampleToDatabase instance for any new raw sample
        // that arrives.
        DLOG(("creating counter for sample id ") << _app.formatId(sampid));
        SampleToDatabase ss(0, sampid);
        _samples[sampid] = ss;
        it = findStats(sampid);
    }
    return it->second.receive(samp);
}



class DataInfluxdb
{
  public:
    DataInfluxdb();

    ~DataInfluxdb() {}

    int run() throw();

    void readHeader(SampleInputStream &sis);

    void readSamples(SampleInputStream &sis);

    int parseRunstring(int argc, char **argv);

    static int main(int argc, char **argv);

    int usage(const char *argv0);

  private:
    static const int DEFAULT_PORT = 30000;

    int _count;

    NidasApp app;
    NidasAppArg Count;
    NidasAppArg URL;
    NidasAppArg Database;
    NidasAppArg Echo;
    NidasAppArg Async;
    NidasAppArg Create;
    NidasAppArg User;
    NidasAppArg Password;

    InfluxDB _db;
};


DataInfluxdb::DataInfluxdb() :
    _count(5000),
    app("data_influxdb"),
    Count("--count", "<count>",
          "Accumulate <count> measurement lines before posting to the "
          "database.  Set to 1 to send each line immediately, the max is 10000.",
          "5000"),
    URL("--url", "<url>",
        "The URL to the influx database.",
        "http://localhost:8086"),
    Database("--db", "<database>",
             "The name of the database.  For EOL weather stations, use "
             "weather_stations."),
    Echo("--echo", "",
         "Echo post data to stdout instead of writing to the database.",
         "false"),
    Async("--async", "{yes|no}",
          "Specify yes to post data to the database asynchronously.",
          "yes"),
    Create("--create", "",
           "Create the given database before posting data to it."),
    User("-u,--user", "username",
         "Username if required for http authentication."),
    Password("-p,--password", "password",
             "Password if required for http authentication."),
    _db()
{
    app.setApplicationInstance();
    app.setupSignals();
    app.enableArguments(app.XmlHeaderFile | app.loggingArgs() | app.Help |
                        app.SampleRanges | app.Version | app.InputFiles |
                        Count | URL | Database | Echo | Create | Async |
                        User | Password);
    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = true;
    app.InputFiles.setDefaultInput("sock:localhost", DEFAULT_PORT);
}

int
DataInfluxdb::
parseRunstring(int argc, char **argv)
{
    // Setup a default log scheme which will get replaced if any logging is
    // configured on the command line.
    n_u::Logger *logger = n_u::Logger::getInstance();
    n_u::LogConfig lc("notice");
    logger->setScheme(logger->getScheme("default").addConfig(lc));

    try
    {
        ArgVector args = app.parseArgs(argc, argv);
        if (app.helpRequested())
        {
            return usage(argv[0]);
        }
        _count = Count.asInt();
        if (_count < 1 || _count > 10000)
        {
            throw NidasAppException("--count must be 1-10000");
        }

        _db.setURL(URL.getValue());
        if (Database.getValue().length() == 0)
        {
            throw NidasAppException("Database name must be "
                                    "specified with --db.");
        }
        _db.setDatabase(Database.getValue());
        _db.setCount(_count);
        _db.setEcho(Echo.asBool());
        _db.setUser(User.getValue(), Password.getValue());
        if (Async.getValue() != "yes" && Async.getValue() != "no")
            throw NidasAppException("--async must be 'yes' or 'no'.");
        _db.setAsync(Async.getValue() == "yes");
        app.parseInputs(args);
    }
    catch (NidasAppException &ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

// TO DO: Change usage information
int
DataInfluxdb::
usage(const char *argv0)
{
    cerr << "Usage: " << argv0 << " [options] [inputURL] ...\n";
    cerr << "Standard options:\n"
         << app.usage() << "Examples:\n"
         << argv0 << " xxx.dat yyy.dat\n"
         << endl;
    return 1;
}

int
DataInfluxdb::
main(int argc, char **argv)
{
    DataInfluxdb didb;
    int result;
    if ((result = didb.parseRunstring(argc, argv)))
    {
        return result;
    }
    return didb.run();
}

class AutoProject
{
  public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

void
DataInfluxdb::
readHeader(SampleInputStream &sis)
{
    // Loop over the header read until it is read or the periods expire.
    // Since the header is not sent until there's a sample to send, if
    // there are no samples we could block in readInputHeader() waiting for
    // the header and never get to the readSamples() loop.
    bool header_read = false;
    while (!header_read && !app.interrupted())
    {
        try
        {
            sis.readInputHeader();
            header_read = true;
        }
        catch (n_u::IOException &e)
        {
            DLOG(("") << e.what() << " (errno=" << e.getErrno() << ")");
            if (e.getErrno() != ERESTART && e.getErrno() != EINTR)
                throw;
        }
        if (app.interrupted())
        {
            throw n_u::Exception("Interrupted while waiting for header.");
        }
    }
}

void
DataInfluxdb::
readSamples(SampleInputStream &sis)
{
    while (!app.interrupted())
    {
        try
        {
            sis.readSamples();
        }
        catch (n_u::IOException &e)
        {
            // reached the end of file, ensure last remaining points are
            // written to the db
            _db.flush();
            DLOG(("") << e.what() << " (errno=" << e.getErrno() << ")");
            ILOG(("") << _db.totalMeasurements()
                 << " measurements written to " << _db.getWriteURL());
            if (e.getErrno() != ERESTART && e.getErrno() != EINTR)
                throw;
        }
    }
}


//both DataInfluxdb and dataDump use similar run()
int DataInfluxdb::run() throw()
{
    int result = 0;
    try
    {
        if (Create.asBool())
        {
            _db.createInfluxDB();
        }

        AutoProject aproject;
        IOChannel *iochan = 0;

        if (app.dataFileNames().size() > 0)
        {
            nidas::core::FileSet *fset =
                nidas::core::FileSet::getFileSet(app.dataFileNames());
            iochan = fset->connect();
        }
        else
        {
            n_u::Socket *sock = new n_u::Socket(*app.socketAddress());
            iochan = new nidas::core::Socket(sock);
        }

        SampleInputStream sis(iochan, /*processed*/true);
        readHeader(sis);

        const SampleInputHeader &header = sis.getInputHeader();

        string xmlFileName = app.xmlHeaderFile();
        if (xmlFileName.length() == 0)
        {
            xmlFileName = header.getConfigName();
            cout << "xmlFileName.length == 0, fileName: " << xmlFileName << "\n";
        }
        xmlFileName = n_u::Process::expandEnvVars(xmlFileName);

        struct stat statbuf;
        if (xmlFileName.length() == 0 ||
            ::stat(xmlFileName.c_str(), &statbuf) != 0)
        {
            throw NidasAppException("xml not specified or cannot be found: " +
                                    xmlFileName);
        }

        n_u::auto_ptr<xercesc::DOMDocument> doc(parseXMLConfigFile(xmlFileName));

        Project::getInstance()->fromDOMElement(doc->getDocumentElement());

        list<DSMSensor *> allsensors;
        DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
        for (; di.hasNext();)
        {
            const DSMConfig *dsm = di.next();
            const list<DSMSensor *> &sensors = dsm->getSensors();
            allsensors.insert(allsensors.end(), sensors.begin(), sensors.end());
        }

        SamplePipeline pipeline;
        SampleDispatcher dispatcher(&_db, allsensors, app);

        pipeline.setRealTime(false);
        pipeline.setRawSorterLength(0);
        pipeline.setProcSorterLength(0);

        list<DSMSensor *>::const_iterator si;
        for (si = allsensors.begin(); si != allsensors.end(); ++si)
        {
            DSMSensor *sensor = *si;
            sensor->init();
            //  1. inform the SampleInputStream of what SampleTags to expect
            sis.addSampleTag(sensor->getRawSampleTag());
        }
        // 2. connect the pipeline to the SampleInputStream.
        pipeline.connect(&sis);

        // 3. connect the client to the pipeline
        pipeline.getProcessedSampleSource()->addSampleClient(&dispatcher);

        try
        {
            while (!app.interrupted())
            {
                readSamples(sis);
            }
        }
        catch (n_u::EOFException &e)
        {
            cerr << e.what() << endl;
        }
        catch (n_u::Exception &e)
        {
            pipeline.getProcessedSampleSource()->removeSampleClient(&dispatcher);
            pipeline.disconnect(&sis);
            pipeline.interrupt();
            pipeline.join();
            sis.close();
            throw(e);
        }
        pipeline.disconnect(&sis);
        pipeline.flush();
        pipeline.getProcessedSampleSource()->removeSampleClient(&dispatcher);
        sis.close();
        pipeline.interrupt();
        pipeline.join();
    }
    catch (n_u::Exception &e)
    {
        cerr << e.what() << endl;
        XMLImplementation::terminate(); 
        result = 1;
    }
    // ok to terminate() twice
    XMLImplementation::terminate();
    return result;
}

int main(int argc, char **argv)
{
    return DataInfluxdb::main(argc, argv);
}
