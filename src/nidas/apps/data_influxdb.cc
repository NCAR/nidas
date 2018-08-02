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
#include <getopt.h>

#include <stdexcept>

#include <fstream> //for writing processed data to a .txt file
#include <string>  //for converting data to_string
#include <sstream>

#include <stdio.h> // for making an api request to insert data into influxdb
#include <curl/curl.h>
#include <curl/easy.h>
#include <future> //async function calls
#include <mutex> //locking the string of data to reset to empty, may not be useful?


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
     * buffer yet.  The returned pointer can be used to print into the
     * buffer, then the buffer length can be extended with advanceBuffer().
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
            _buffer.resize(_buflen + length + 1);
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
};



// std::mutex mtx;



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
        _data(),
        _nmeasurements(0),
        _count(1),
        _echo(false),
        _curl(0)
    {
        _curl = curl_easy_init();
        if (! _curl)
        {
            throw std::runtime_error("curl_easy_init failed.");
        }
    }

    void
    setURL(const std::string& url)
    {
        _url = url;
    }
        
    void
    setDatabase(const std::string& dbname)
    {
        _dbname = dbname;
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
        return out.str();
    }

    void
    addMeasurement(const std::string& data)
    {
        char* buf = _data.getSpace(data.length());
        strcat(buf, data.c_str());
        ++_nmeasurements;
        if (_nmeasurements >= _count)
        {
            sendData();
        }            
    }

    void
    sendData()
    {
        if (_data.empty())
        {
            return;
        }
        DLOG(("sendData()..."));
        if (_echo)
        {
            std::cout << _data.get();
        }
        else
        {
            // std::launch::async forces it to launch in parallel (as
            // opposed to the default of sequentially)

            // std::future<void> call = std::async(std::launch::async,
            //     &SampleToDatabase::dataToInfluxDB, this, multipleData);

            // wait for async processing of each line of data to finish
            // before clearing the multipleData string

            // mtx.lock();
            // string dbdata = multipleData;
            // multipleData = "";
            // mtx.unlock();
            // cout << multipleData.size() << "\n";
            std::async(std::launch::async, &InfluxDB::dataToInfluxDB, this);
        }
        _nmeasurements = 0;
    }

    void
    createInfluxDB();

    void
    dataToInfluxDB();

private:

    InfluxDB(const InfluxDB&);
    InfluxDB& operator=(const InfluxDB&);

    string _url;
    string _dbname;

    CharBuffer _data;
    unsigned int _nmeasurements;
    unsigned int _count;
    bool _echo;
    
    CURL *_curl;
};


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


bool SampleToDatabase::
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

    string url = getHostURL() + "/query";
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
}


/**
 * Post the current data buffer to the database, then clear it.
 **/
void
InfluxDB::
dataToInfluxDB()
{
    CURLcode res;
    
    string url = getWriteURL();
    curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, _data.get());
    DLOG(("posting ") << _nmeasurements << " measurements to database: " << url);
    res = curl_easy_perform(_curl);
    _data.clear();
    if (res != CURLE_OK)
    {
        ELOG(("database write failed with code: ") << res);
    }
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
            //TO DO: should we have a boolean field set to true is the value is an NaN? write to db? or just continue?
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
            _db->addMeasurement(data);
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
    NidasAppArg Create;

    InfluxDB _db;
};


//TO DO: modify to be accurate of what DataInfluxdb can do
DataInfluxdb::DataInfluxdb() :
    _count(5000),
    app("data_influxdb"),
    Count("--count", "<count>",
          "Accumulate <count> measurement lines before posting to the "
          "database.  Set to 1 to send each line immediately, the max is 5000.",
          "5000"),
    URL("--url", "<url>",
        "The URL to the influx database.",
        "http://localhost:8086"),
    Database("--db", "<database>",
             "The name of the database.",
             "weather_stations_units"),
    Echo("--echo", "",
         "Echo post data to stdout instead of writing to the database.",
         "false"),
    Create("--create", "",
           "Create the given database before posting data to it."),
    _db()
{
    app.setApplicationInstance();
    app.setupSignals();
    app.enableArguments(app.XmlHeaderFile | app.loggingArgs() |
                        app.SampleRanges | 
                        app.Version | app.InputFiles |
                        app.Help | Count | URL | Database | Echo | Create);
    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = true;
    app.InputFiles.setDefaultInput("sock:localhost", DEFAULT_PORT);
}

int DataInfluxdb::parseRunstring(int argc, char **argv)
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
        if (_count < 1 || _count > 5000)
        {
            throw NidasAppException("--count must be 1-5000");
        }

        _db.setURL(URL.getValue());
        _db.setDatabase(Database.getValue());
        _db.setCount(_count);
        _db.setEcho(Echo.asBool());
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
int DataInfluxdb::usage(const char *argv0)
{
    cerr << "Usage: " << argv0 << " [options] [inputURL] ...\n";
    cerr << "Standard options:\n"
         << app.usage() << "Examples:\n"
         << argv0 << " xxx.dat yyy.dat\n"
         << endl;
    return 1;
}

int DataInfluxdb::main(int argc, char **argv)
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

void DataInfluxdb::
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

void DataInfluxdb::
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
            _db.sendData();
            DLOG(("") << e.what() << " (errno=" << e.getErrno() << ")");
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
        sis.setMaxSampleLength(32768);
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
        catch (n_u::IOException &e)
        {
            pipeline.getProcessedSampleSource()->removeSampleClient(&dispatcher);
            pipeline.disconnect(&sis);
            pipeline.interrupt();
            pipeline.join();
            sis.close();
            XMLImplementation::terminate();
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
