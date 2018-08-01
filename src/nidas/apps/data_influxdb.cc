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

/*
TO DO: --BARNITZ--
*obtain the variable units for each if necessary for visualization,

*set command line arguments
*set the private variable to process data //currently set with command line arguements (CLA)
*have the app run as own process in the background //currently invoking each instant with CLA

*determine application name //currently set to data_parse_values (not very meaningful)
*update headers and object/method names //currently heavily relied on what was referenced with data_stats
*remove code and functions/methods that serve no purpose for this application
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

//these globals will be omitted (for testing purposes)
int COUNT = 0;
string multipleData;

string data;
time_t start_time = time(&start_time);

class SampleToDatabase
{
//   private: 
//     class dbSenderThread //inherit from boost lib
//         static threadPool;  //thread pool buffer
  
  public:
    /**
     * A default constructor is required to use objects as a map element.
     **/
    SampleToDatabase(dsm_sample_id_t sid = 0, const std::string &sname = "",
                     const SampleTag *stag = 0) : name(sname),
                                                  id(sid),
                                                  sitename(),
                                                //   DBname("grainex3"),
                                                  DBname("weather_stations"),
                                                  measurementName(),
                                                  spsid(),
                                                  dsmid(),
                                                  info(),
                                                  url("http://snoopy.eol.ucar.edu:8086/write?db=" + DBname + "&precision=u"),
                                                //   collectionOfData(""),
                                                  varnames()
    {
        if (stag)
        {
            const std::vector<const Variable *> &variables = stag->getVariables();
            for (unsigned int i = 0; i < variables.size(); ++i)
            {
                varnames.push_back(variables[i]->getName());
            }
            //TO DO: determine & implement how best to incorporate this change necessary for the different sites: given as command line arguements
            //sitename added to constructor, stripped the _ from the suffix to use as the MEASUREMENT name in the influx database
            //necessary for the weather stations
            //EOL-WEATHER-STATIONS
            sitename = stag->getSuffix();
            measurementName = sitename.erase(0,1);

            //for conventional nidas configurations the site name should come from DSMSensor::getSite()->getName()
            //necessary for grainex/conventional projects
            //CONVENTIONAL PROJECTS
            sitename = stag->getSite()->getName();
            dsmid = to_string(stag->getDSMId());
            int tempSpSid = stag->getSpSId();
            if (tempSpSid >= 0x8000)
            {
                stringstream intToHex;
                intToHex << "0x" << hex << tempSpSid;
                spsid = intToHex.str();
            }
            else
                spsid = to_string(tempSpSid);
            // //this measurement name is not used currently for weather_stations, weather_stations use the sitename
            // measurementName = "dsmid:" + dsmid + ".spsid:" + spsid;
            info = measurementName + ",dsm_id=" + dsmid + ",location=" + sitename + ",sps_id=" + spsid + " ";
        }
    }
    bool
    receive(const Sample *samp) throw();

    void
    createInfluxDB();

    void
    dataToInfluxDB(string data);

    void
    accumulate(const Sample *samp);

    string name;
    dsm_sample_id_t id;

    // Stash the site name from the sample tag to identify the
    // site in the accumulated data.
    string sitename;

    // Database name for the influxdb specific to the project
    string DBname;

    // To store the measurement name comprised of dsm id and sensor id concatenated
    // so that it is unique for each variable
    string measurementName;

    //gathering dsmid and spsid necessary to create measurement name
    string spsid;
    string dsmid;
    string info;
    string url;

    // Stash the variable names from the sample tag to identify the
    // variables in the accumulated data.
    vector<string> varnames;
};

std::future<void> beforeResettingString;
std::mutex mtx;

bool SampleToDatabase::
    receive(const Sample *samp) throw()
{
    if (samp->getType() != FLOAT_ST && samp->getType() != DOUBLE_ST)
    {
        return true;
    }
    //spawn a new thread for each sample
    //consumes more resources then necessary without gain in computation time
    // std::async(std::launch::async, &SampleToDatabase::accumulate, this, samp);   
    accumulate(samp);
    // if ((difftime(time(NULL), start_time) > 2) && COUNT > 5000)
    if (COUNT > 5000)
    {
        // std::launch::async forces it to launch in parallel (as opposed to the default of sequentially)
        // std::future<void> call = std::async(std::launch::async, &SampleToDatabase::dataToInfluxDB, this, multipleData);
        // wait for async processing of each line of data to finish before clearing the multipleData string
        // mtx.lock();
        // string dbdata = multipleData;
        // multipleData = "";
        // mtx.unlock();
        // cout << multipleData.size() << "\n";
        std::async(std::launch::async, &SampleToDatabase::dataToInfluxDB, this, multipleData);
        multipleData = "";
        COUNT = 0;
    }
    return true;
}

/*
 * This is invoked from main() at the initialization of the data to be transferred to the influx database. This function creates an influx database at 
 * the specfied url, which can be either "http://localhost:8086/query" if the influx database is on the same server, or the full url can be provided 
 * if it is located on different servers (ensuring correct firewall permissions are set). createInfluxDB() uses the curl library for assigning the url 
 * and the consequent fields to post during the HTTP POST operation. Additionally, the string variable DBname can be assigned with the DSMConfig 
 * information such as location or project name as this is only created once //for now it is passed as a parameter from main
 */
void SampleToDatabase::
    createInfluxDB()
{
    //TO DO: make database name variable assigned depending on location/project? should it be given as command line arguement? currently set in the constructor.
    //cout << "in create influxdb the db name is: " << DBname << "\n";
    CURL *curl;
    CURLcode res;

    const char *url = "http://snoopy.eol.ucar.edu:8086/query";
    string createDB = "q=CREATE+DATABASE+" + DBname;

    curl = curl_easy_init();

    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, createDB.c_str());

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            cout << "failed\n";

        curl_easy_cleanup(curl);
    }
}

void SampleToDatabase::
dataToInfluxDB(string dbdata)
{
    // string url = "http://snoopy.eol.ucar.edu:8086/write?db=" + DBname + "&precision=u";//.c_str();
    // const char* dbdata = data.c_str();
    // std::async(std::launch::async,[=](){
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    
    if (curl){
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, dbdata.c_str());
        res = curl_easy_perform(curl);
        if (res != CURLE_OK){
            cout << "failed\n" << "because: " << res << "\n";
            //should attempt to write data/ best use of error handling?
        }
        curl_easy_cleanup(curl);
    }
    // });
}


void SampleToDatabase::
accumulate(const Sample *samp)
{
    if (samp->getType() != FLOAT_ST && samp->getType() != DOUBLE_ST)
    {
        return;
    }

    unsigned int nvalues = varnames.size();

    // where info =  measurementName + ",location=" + sitename + ",dsm_id=" + dsmid + ",sps_id=" + spsid + " "
    //created in the constructor
    data = info;
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
            data += varnames[i];
            data += "=";
            data += to_string(value);
            data += ",";
        }
    }
    char lastChar = data.back();
    if (lastChar == ',')
    {
        data.pop_back();
    }
    data += " ";
    data += to_string(samp->getTimeTag());
    data += "\n";
    multipleData += data;
    ++COUNT;
}


class CounterClient : public SampleClient
{
  public:
    CounterClient(const list<DSMSensor *> &sensors, NidasApp &app);

    virtual ~CounterClient() {}

    void flush() throw() {}

    bool receive(const Sample *samp) throw();

    void
    reportAll(bool all)
    {
        _reportall = all;
    }

    void
    reportData(bool data)
    {
        _reportdata = data;
    }

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

    bool _reportall;

    bool _reportdata;

    NidasApp &_app;
};

// void
// CounterClient::
// resetResults()
// {
//     cout << "... CounterClient::resetResults\n";
//     cout << "...do i need this? 5\n";
//     sample_map_t::iterator si;
//     for (si = _samples.begin(); si != _samples.end(); ++si)
//     {
//         si->second.reset();
//     }
// }

CounterClient::CounterClient(const list<DSMSensor *> &sensors, NidasApp &app) : _samples(),
                                                                                _reportall(false),
                                                                                _reportdata(false),
                                                                                _app(app)
{
    cout << "... CounterClient::CounterClient\n";
    bool processed = app.processData();
    //cout << "... process data? " << processed << "\n";
    SampleMatcher &matcher = _app.sampleMatcher();
    list<DSMSensor *>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si)
    {
        // Create a SampleToDatabase for samples from the given sensors.  Raw
        // samples are named by the sensor device, processed samples by the
        // first variable in the first sample tag.
        DSMSensor *sensor = *si;
        string sname = sensor->getDSMConfig()->getName() + ":" +
                       sensor->getDeviceName();

        // Stop with raw samples if processed not requested.
        if (!processed)
        {
            if (matcher.match(sensor->getId()))
            {
                dsm_sample_id_t sid = sensor->getId();
                cout << "... DLOG:adding raw sample with sid: " << sid << " with sname:" << sname << "\n";
                DLOG(("adding raw sample: ") << _app.formatId(sid));
                SampleToDatabase stats(sid, sname);
                _samples[stats.id] = stats;
            }
            continue;
        }

        // for samples show the first variable name, followed by ",..."
        // if more than one.
        SampleTagIterator ti = sensor->getSampleTagIterator();
        for (; ti.hasNext();)
        {
            const SampleTag *stag = ti.next();

            if (stag->getVariables().size() > 0)
            {
                string varname = stag->getVariables().front()->getName();
                if (stag->getVariables().size() > 1)
                {
                    varname += ",...****BARNITZ****";
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
                    //cout << "... DLOG((adding processed sample: ) with _app.formatId(sid)): " << _app.formatId(sid) << "\n";
                    DLOG(("adding processed sample: ") << _app.formatId(sid));
                    SampleToDatabase pstats(sid, varname, stag);
                    _samples[pstats.id] = pstats;
                }
            }
        }
    }
}

bool CounterClient::receive(const Sample *samp) throw()
{
    //cout << "... CounterClient::receive\n";
    //cout << "...do i need this? 4\n";
    dsm_sample_id_t sampid = samp->getId();
    if (!_app.sampleMatcher().match(sampid))
    {
        return false;
    }
    //cout << "... VLOG: received and accepted sample\n";
    VLOG(("received and accepted sample ") << _app.formatId(sampid));
    sample_map_t::iterator it = findStats(sampid);
    if (it == _samples.end())
    {
        // When there is no header from which to gather samples ahead of
        // time, just add a SampleToDatabase instance for any new raw sample
        // that arrives.
        DLOG(("creating counter for sample id ") << _app.formatId(sampid));
        SampleToDatabase ss(sampid);
        _samples[sampid] = ss;
        it = findStats(sampid);
    }
    //cout << "...fin\n";
    return it->second.receive(samp);
}



class DatabaseData
{
  public:
    DatabaseData();

    ~DatabaseData() {}

    int run() throw();

    void readHeader(SampleInputStream &sis);

    void readSamples(SampleInputStream &sis);

    int parseRunstring(int argc, char **argv);

    static int main(int argc, char **argv);

    int usage(const char *argv0);

    bool
    reportsExhausted(int nreports = -1)
    {
        // Just to avoid the unused warning, while allowing _nreports to be
        // incremented with a prefix increment operator in the call to this
        // method.
        if (nreports > -1)
            _nreports = nreports;
        return (_count > 0 && _nreports > _count);
    }

    static void handleSignal(int signum);

  private:
    static const int DEFAULT_PORT = 30000;

    static bool _alarm;
    bool _realtime;
    n_u::UTime _period_start;
    int _count;
    int _period;
    int _nreports;

    NidasApp app;
    NidasAppArg Period;
    NidasAppArg Count;
    // Type of report to generate:
    //
    // All - show all samples, received or not
    // Missing - show only missing samples
    // Compact - report only one line for a site with no samples for any sensors
    // Received - show only received samples, the default
    NidasAppArg AllSamples;

    // Show averaged data or raw messages for each report.
    NidasAppArg ShowData;
};

bool DatabaseData::_alarm(false);

void DatabaseData::handleSignal(int signum)
{
    // The NidasApp handler sets interrupted before calling this handler,
    // so clear that if this is just the interval alarm.
    if (signum == SIGALRM)
    {
        NidasApp::setInterrupted(false);
        _alarm = true;
    }
}

//TO DO: modify to be accurate of what DatabaseData can do
DatabaseData::DatabaseData() : _realtime(false), _period_start(time_t(0)),
                         _count(1), _period(0), _nreports(0),
                         app("data_stats"),
                         Period("-P,--period", "<seconds>",
                                "Collect statistics for the given number of seconds and then "
                                "print the report.\n"
                                "If 0, wait until interrupted with Ctl-C.",
                                "0"),
                         Count("-n,--count", "<count>",
                               "When --period specified, generate <count> reports.\n"
                               "Use a count of zero to continue reports until interrupted.",
                               "1"),
                         AllSamples("-a,--all", "",
                                    "Show statistics for all sample IDs, including those for which "
                                    "no samples are received."),
                         ShowData("-D,--data", "",
                                  "Print data for each sensor, either the last received message\n"
                                  "for raw samples, or data values averaged over the recording\n"
                                  "period for processed samples.")
{
    //cout << "... DatabaseData::DatabaseData\n";
    app.setApplicationInstance();
    app.setupSignals();
    app.enableArguments(app.XmlHeaderFile | app.LogConfig |
                        app.SampleRanges | app.FormatHexId |
                        app.FormatSampleId | app.ProcessData |
                        app.Version | app.InputFiles |
                        app.Help | Period | Count | AllSamples | ShowData);
    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = true;
    app.InputFiles.setDefaultInput("sock:localhost", DEFAULT_PORT);
}

int DatabaseData::parseRunstring(int argc, char **argv)
{
    //cout << "... DatabaseData::parseRunString\n";
    // Setup a default log scheme which will get replaced if any logging is
    // configured on the command line.
    n_u::Logger *logger = n_u::Logger::getInstance();
    n_u::LogConfig lc("notice");
    logger->setScheme(logger->getScheme("default").addConfig(lc));

    try
    {
        ArgVector args = app.parseArgs(argc, argv);
        cout << "...try of DatabaseData:: parseRunString\n";
        if (app.helpRequested())
        {
            return usage(argv[0]);
        }
        _period = Period.asInt();
        _count = Count.asInt();

        app.parseInputs(args);
        //set process data to true? update the default to true for this application
        //app._processData = true;
    }
    catch (NidasAppException &ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
// TO DO: Change usage information
int DatabaseData::usage(const char *argv0)
{
    cout << "... DatabaseData::usage\n";
    cerr << "Usage: " << argv0 << " [options] [inputURL] ...\n";
    cerr << "Standard options:\n"
         << app.usage() << "Examples:\n"
         << argv0 << " xxx.dat yyy.dat\n"
         << argv0 << " file:/tmp/xxx.dat file:/tmp/yyy.dat\n"
         << argv0 << " -p -x ads3.xml sock:hyper:30000\n"
         << endl;
    return 1;
}

int DatabaseData::main(int argc, char **argv)
{
    DatabaseData stats;
    multipleData.reserve(1500000);
    multipleData = "";
    int result;
    if ((result = stats.parseRunstring(argc, argv)))
    {
        cout << "main if statement\n";
        return result;
    }
    cout << "main not if statement\n";
    //SampleToDatabase sc;
    //TO DO: is this the best place for invoking the creation of a new database? potential for using CLA for assigning the DB a name, or obtaining that info from the sensor?
    //string DBname initialized in the constructor, and the first instantiation should be used continously thereafter
    // string DBNameAssigned = "";
    // cout << "Please enter a database name: ";
    // getline(cin, DBNameAssigned);
    // sc.DBname = DBNameAssigned;
    //sc.createInfluxDB();
    return stats.run();
}

class AutoProject
{
  public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

void DatabaseData::
    readHeader(SampleInputStream &sis)
{
    // Loop over the header read until it is read or the periods expire.
    // Since the header is not sent until there's a sample to send, if
    // there are no samples we could block in readInputHeader() waiting for
    // the header and never get to the readSamples() loop.
    bool header_read = false;
    _nreports = 0;
    while (!header_read && !app.interrupted() &&
           !reportsExhausted(++_nreports))
    {
        _alarm = false;
        if (_realtime)
            alarm(_period);
        try
        {
            sis.readInputHeader();
            header_read = true;
            // Reading the header does not count as a report cycle.
            --_nreports;
        }
        catch (n_u::IOException &e)
        {
            DLOG(("") << e.what() << " (errno=" << e.getErrno() << ")");
            if (e.getErrno() != ERESTART && e.getErrno() != EINTR)
                throw;
        }
        if (_realtime)
            alarm(0);
        if (app.interrupted())
        {
            throw n_u::Exception("Interrupted while waiting for header.");
        }
        if (_alarm)
        {
            ostringstream outs;
            outs << "Header not received after " << _nreports
                 << " periods of " << _period << " seconds.";
            // Throw an exception if nreports exhausted.
            if (reportsExhausted())
            {
                throw n_u::Exception(outs.str());
            }
            else
            {
                cerr << outs.str() << endl;
            }
        }
    }
}

void DatabaseData::
    readSamples(SampleInputStream &sis)
{
    cout << "...DatabaseData::readSamples\n";
    // Read samples until an alarm signals the end of a reporting period or
    // an interruption occurs.
    _alarm = false;
    if (_period > 0 && _realtime)
    {
        alarm(_period);
    }
    cout << "...after if in readSamples\n";
    while (!_alarm && !app.interrupted())
    {
        //cout << "...in while of readSamples\n";
        try
        {
            //cout << "...in try of readSamples\n";
            sis.readSamples();
        }
        catch (n_u::IOException &e)
        {
            cout << "...in catch of readSamples\n";
            //reached the end of file, ensure last remaining points are written to the db
            SampleToDatabase sd;
            sd.dataToInfluxDB(multipleData);
            DLOG(("") << e.what() << " (errno=" << e.getErrno() << ")");
            if (e.getErrno() != ERESTART && e.getErrno() != EINTR)
                throw;
        }
    }
}


//both DatabaseData and dataDump use similar run()
int DatabaseData::run() throw()
{
    int result = 0;
    cout << "...DatabaseData::run\n";
    try
    {
        cout << "...try\n";
        AutoProject aproject;
        IOChannel *iochan = 0;

        if (app.dataFileNames().size() > 0)
        {
            cout << "... if there is a dataFileName\n";
            nidas::core::FileSet *fset =
                nidas::core::FileSet::getFileSet(app.dataFileNames());
            iochan = fset->connect();
        }
        else
        {
            n_u::Socket *sock = new n_u::Socket(*app.socketAddress());
            iochan = new nidas::core::Socket(sock);
            _realtime = true; //in data stats not in data dump //necessary?
        }

        SampleInputStream sis(iochan, app.processData());
        sis.setMaxSampleLength(32768);
        // sis.init();

        if (_period > 0 && _realtime)
        {
            cout << "... if _period >0 && _realtime\n";
            app.addSignal(SIGALRM, &DatabaseData::handleSignal, true);
        }
        readHeader(sis);

        const SampleInputHeader &header = sis.getInputHeader();

        list<DSMSensor *> allsensors;

        string xmlFileName = app.xmlHeaderFile();
        //cout << "... xmlFileName: *" << xmlFileName << "*\n";
        //if the file name is not given it will automatically be assigned to ****/weather-eol-stations.xml
        if (xmlFileName.length() == 0)
        {
            xmlFileName = header.getConfigName();
            cout << "xmlFileName.length == 0, fileName: " << xmlFileName << "\n";
        }
        xmlFileName = n_u::Process::expandEnvVars(xmlFileName);
        //TO DO: change this hard coding of file name //used because of trying to bypass giving CLA for processing data
        //xmlFileName = "/h/eol/jbarnitz/barnitzSUPER/eol-weather-stations-TEMP2.xml"; //...change this
        //cout << "xmlFileName.length == 0, fileName: " << xmlFileName <<"\n";
        //cout << "... app.processData(): " << app.processData() << "\n";
        struct stat statbuf;
        //we should want to process all data that comes in to store in the DB as "raw" but processed data s.t. it is no longer ascii
        //thus use the .xml file for this
        if (::stat(xmlFileName.c_str(), &statbuf) == 0 || app.processData())
        {
            n_u::auto_ptr<xercesc::DOMDocument>
                doc(parseXMLConfigFile(xmlFileName));

            Project::getInstance()->fromDOMElement(doc->getDocumentElement());

            DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
            for (; di.hasNext();)
            {
                const DSMConfig *dsm = di.next();
                const list<DSMSensor *> &sensors = dsm->getSensors();
                allsensors.insert(allsensors.end(), sensors.begin(), sensors.end());
            }
        }
        XMLImplementation::terminate();

        SamplePipeline pipeline;
        CounterClient counter(allsensors, app); //not in data_dump
        counter.reportAll(AllSamples.asBool());
        counter.reportData(ShowData.asBool());

        if (app.processData())
        {
            cout << "... app.processData() is true\n";
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
            pipeline.getProcessedSampleSource()->addSampleClient(&counter);
            pipeline.getRawSampleSource()->addSampleClient(&counter); //maybe??
        }
        else
            sis.addSampleClient(&counter);

        try
        {
            if (_period > 0 && _realtime)
            {
                cout << "....... Collecting samples for " << _period << " seconds "
                     << "......." << endl;
            }
            while (!app.interrupted() && !reportsExhausted(++_nreports))
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
            if (app.processData())
            {
                pipeline.getProcessedSampleSource()->removeSampleClient(&counter);
                pipeline.disconnect(&sis);
                pipeline.interrupt();
                pipeline.join();
            }
            else
            {
                pipeline.getRawSampleSource()->removeSampleClient(&counter); //maybe??
                //sis.removeSampleClient(&counter);
            }
            sis.close();
            throw(e);
        }
        if (app.processData())
        {
            pipeline.disconnect(&sis);
            pipeline.flush();
            pipeline.getProcessedSampleSource()->removeSampleClient(&counter);
            pipeline.getRawSampleSource()->removeSampleClient(&counter); //maybe???
        }
        else
        {
            sis.removeSampleClient(&counter);
        }
        sis.close();
        pipeline.interrupt();
        pipeline.join();
    }
    catch (n_u::Exception &e)
    {
        cerr << e.what() << endl;
        XMLImplementation::terminate(); // ok to terminate() twice
        result = 1;
    }
    return result;
}

int main(int argc, char **argv)
{
    return DatabaseData::main(argc, argv);
}
