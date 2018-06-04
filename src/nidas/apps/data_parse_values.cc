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
*set the private variable to process data //currently set with command line arguements (CLA)
*ensure the .xml file is used when processing the data //currently given in CLA
*have the app run as own process in the background //currently invoking each instant with CLA and a specified period

*implement adding data to influxdb based on the attribute (ie temp, wind_dir, rain_accum) //currently it is added as site specific with all attributes for given timestamp
*consider batching/async adding of data to influx db (hand in hand with above to do item) //currently call influxdb for each timestamp of data for every site

*determine application name //currently set to data_parse_values (not very meaningful)
*update headers and object/method names //currently heavily relied on what was referenced with data_stats
*remove code and functions/methods that serve no purpose for this application

*begin working on derivations of data in the influx db //see /net/weather/weather/src/ccwd and apply every five min to data in influx db
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
#include <string> //for converting data to_string

#include <stdio.h> // for making an api request to insert data into influxdb
#include <curl/curl.h>
#include <curl/easy.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class SampleCounter
{
public:
    /**
     * A default constructor is required to use objects as a map element.
     **/
    SampleCounter(dsm_sample_id_t sid = 0, const std::string& sname = "",
                  const SampleTag* stag = 0) :
        name(sname),
        id(sid),
        t1s(0),
        t2s(0),
        nsamps(0),
        minlens(0),
        maxlens(0),
        minDeltaTs(0),
        maxDeltaTs(0),
        sums(),
        nnans(),
        rawmsg(),
        varnames()
    {
        if (stag)
        {
            const std::vector<const Variable*>& variables = stag->getVariables();
            for (unsigned int i = 0; i < variables.size(); ++i)
            {
                varnames.push_back(variables[i]->getName());
            }
//TO DO: add suffix variable to the constructor stripping the _ from the suffixto use as the MEASUREMENT name in the influx database
            //for conventional nidas configurations the site name should come from DSMSensor::getSite()->getName()
        
            //siteName = stag->getSuffix()
            //siteName.erase(siteName[0]);
            cout << "********** stag->getSuffix: " << stag->getSuffix() << "\n"; 
        }
    }

    void
    reset()
    {
        t1s = 0;
        t2s = 0;
        nsamps = 0;
        minlens = 0;
        maxlens = 0;
        minDeltaTs = 0;
        maxDeltaTs = 0;
        sums.clear();
        nnans.clear();
        rawmsg.erase();
    }

    bool
    receive(const Sample* samp) throw();

    void
    accumulate(const Sample* samp);

    string name;
    dsm_sample_id_t id;

    dsm_time_t t1s;
    dsm_time_t t2s;
    size_t nsamps;
    size_t minlens;
    size_t maxlens;
    int minDeltaTs;
    int maxDeltaTs;

    // Accumulate sums of each variable in the sample and counts of the
    // number of nans seen in each variable.
    vector<float> sums;
    vector<int> nnans;

    // For raw samples, just keep the last message seen.  Someday this
    // could be a buffer of the last N messages.
    std::string rawmsg;

    // Stash the variable names from the sample tag to identify the
    // variables in the accumulated data.
    vector<string> varnames;

    // // Stash the site names from the sample tag to identify the
    // // site in the accumulated data.
    // vector<string> siteName;
};


bool
SampleCounter::
receive(const Sample* samp) throw()
{
    //cout << "... SampleCounter::receive\n";
    dsm_sample_id_t sampid = samp->getId();

    VLOG(("counting sample ") << nsamps << " for id "
         << NidasApp::getApplicationInstance()->formatId(sampid));
    if (sampid != id)
    {
        ILOG(("assigning received sample ID ")
             << NidasApp::getApplicationInstance()->formatId(sampid)
             << " in place of "
             << NidasApp::getApplicationInstance()->formatId(id));
        id = sampid;
    }
    dsm_time_t sampt = samp->getTimeTag();
    if (nsamps == 0)
    {
        t1s = sampt;
        minDeltaTs = INT_MAX;
        maxDeltaTs = INT_MIN;
    }
    else
    {
        int deltaT = (sampt - t2s + USECS_PER_MSEC/2) / USECS_PER_MSEC;
	minDeltaTs = std::min(minDeltaTs, deltaT);
	maxDeltaTs = std::max(maxDeltaTs, deltaT);
    }
    t2s = sampt;

    size_t slen = samp->getDataByteLength();
    if (nsamps == 0)
    {
        minlens = slen;
        maxlens = slen;
    }
    else
    {
        minlens = std::min(minlens, slen);
        maxlens = std::max(maxlens, slen);
    }
    ++nsamps;

    accumulate(samp);
    return true;
}

/*
This is invoked from main() at the initialization of the data to be transferred to the influx database. This function creates an influx database at 
the specfied url, which can be either "http://localhost:8086/query" if the influx database is on the same server, or the full url can be provided 
if it is located on different servers (ensuring correct firewall permissions are set). createInfluxDB() uses the curl library for assigning the url 
and the consequent fields to post during the HTTP POST operation. Additionally, the string variable DBname can be assigned with the DSMConfig 
information such as location or project name as this is only created once //for now it is hard coded as "test"
*/
void createInfluxDB(){
//TO DO: make database name variable assigned depending on location/project 
    //string DBname = "test";
    CURL *curl;
    CURLcode res;

    const char* url = "http://snoopy.eol.ucar.edu:8086/query";
    const char* createDB = "q=CREATE+DATABASE+test1";
    
    curl = curl_easy_init();

    if (curl){

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, createDB);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK)
            cout << "failed\n";

        curl_easy_cleanup(curl);
    }
    cout << "end of create influx db\n";
}

/*
dataToInfluxDB() inserts the processed data into the influxdb with the curl library for the given timestamp, specifying a precision of microseconds in the url 
(the default of influxDB is nanoseconds). The HTTP POST necessitates that the data string is given as a char*  
*/
void dataToInfluxDB(string data){
    CURL *curl;
    CURLcode res;

    const char *url = "http://snoopy.eol.ucar.edu:8086/write?db=test1&precision=u"; // precision=u for microsecond precision as opposed to default nanaosecond
    curl = curl_easy_init();

    if (curl){
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

        res = curl_easy_perform(curl);

        if(res != CURLE_OK)
            cout << "failed\n";

        curl_easy_cleanup(curl);
    }
    cout << "end of add data synchronously to influx db\n";
}

/*
this version of dataToInfluxDB() adds the data line by line from a file that is written to in SampleCounter::accumulate
//intention of removing this code completely, but first want to determine the best options for adding batched data to influxdb  
*/
// void dataToInfluxDB(){
//     CURL *curl;
//     CURLcode res;

//     const char *url = "http://snoopy.eol.ucar.edu:8086/write?db=test1&precision=u"; // precision=u for microsecond precision as opposed to default nanaosecond
//     //write to db with entire parsing of a file
//     ifstream dataFile;
//     string line;
//     dataFile.open("/h/eol/jbarnitz/barnitzSUPER/test.txt");

//     while (getline(dataFile, line)){
//         //cout << line << "\n";
//         curl = curl_easy_init();

//         if (curl){
//             curl_easy_setopt(curl, CURLOPT_URL, url);
//             curl_easy_setopt(curl, CURLOPT_POSTFIELDS, line.c_str());

//             res = curl_easy_perform(curl);

//             if(res != CURLE_OK)
//                 cout << "failed\n";

//             curl_easy_cleanup(curl);
//         }
//     }
//     dataFile.close();
//     cout << "end of add data from a file to influx db\n";
// }

void
SampleCounter::
accumulate(const Sample* samp)
{
//TO DO: is this needed for adding to the influxdb? //perhaps better understand the catalyst of changing from a sample of type char to type float 
    //cout << "... SampleCounter::accumulate\n";
    if (samp->getType() == CHAR_ST)
    {
        const char* cp = (const char*)samp->getConstVoidDataPtr();
        size_t l = samp->getDataByteLength();
        if (l > 0 && cp[l-1] == '\0') l--;  // exclude trailing '\0'
        rawmsg = n_u::addBackslashSequences(string(cp,l));
        //getting each read in piece of raw data from here
        //cout << "...rawmsg line 203: " << rawmsg << "\n";
        return;
    }
    if (samp->getType() != FLOAT_ST && samp->getType() != DOUBLE_ST)
    {
        return;
    }
    unsigned int nvalues = samp->getDataLength();
    //at this point we should add the values to the influxDB
    //cout << "... SampleCounter::accumulate samp->getType() is of type FLOAT or DOUBLE\n";
    if (nvalues > sums.size())
    {
        sums.resize(nvalues);
        nnans.resize(nvalues);
    }

    for (unsigned int i = 0; i < nvalues; ++i)
    {
        double value = samp->getDataValue(i);
//TO DO: determine whether to create own bool isnan function or if using std:: is okay as there are portability issues with c++11 and prior versions
        if (std::isnan(value))
        {
            nnans[i] += 1;
        }
        else
        {
            sums[i] += value;
        }
        // cout << "...at index i: " << i << ", varname is: " << varnames[i] << " with value of: " << value << "\n";
    }

//TO DO: remove the writing to file once async/batching of writing data to influxdb has been determined
//     ofstream testWritingData;
//     testWritingData.open("/h/eol/jbarnitz/barnitzSUPER/test.txt", ios::out | ios::app);
//     int dsmId = samp->getDSMId();
//     int spSid = samp->getSpSId();
// //TO DO: access the suffix appropriately need a const sampleTag* stag and then stag->getSuffix() which yields _fl, _ml, _raf ect handle in constructor 
//     string location;

//     if (spSid==11)
//         location = "fl";
//     else if (spSid==21)
//         location = "ml";
//     else if (spSid==31)
//         location = "nsf";
//     else if (spSid==41)
//         location = "nwsc";
//     else if (spSid==51)
//         location = "raf";
//     else if (spSid==61)
//         location = "fireside";
//     else
//         location = "mlw";

//     if (testWritingData.is_open()){
//         testWritingData << location << ",location=" << location << ",dsm_id=" << dsmId << ",sensor_id=" << spSid << " ";            
//     }

//     for (unsigned int i = 0; i < nvalues; ++i)
//     {
//         double value = samp->getDataValue(i);
//         if (std::isnan(value))
//         {
// //TO DO: should we have a boolean field set to true is the value is an NaN? write to db? or just continue?
//             // cout << varnames[i] << " is " << value << "\n";
//             continue;
//         }
//         else
//         {
//             if (testWritingData.is_open()){
//                 if (i < nvalues-1){
//                     testWritingData << varnames[i] << "=" << value << ",";
//                 }
//                 else{
//                     testWritingData << varnames[i] << "=" << value << " " << samp->getTimeTag() << "\n";
//                 }
//             }
//             //write to influx db with location, dsmID, sensorID, variable name, value, ....
//             //cout << varnames[i] << " with value " << value << " **** TO BE WRITTEN TO INFLUXDB ****\n";
//         }
//     }
//     testWritingData.close();

    //try synchronous insert into influxdb
    int dsmId = samp->getDSMId(); 
    int spSid = samp->getSpSId();
// //TO DO: access the suffix appropriately need a const sampleTag* stag and then stag->getSuffix() which yields _fl, _ml, _raf ect handle in constructor 
    string location;
    if (spSid==11)
        location = "fl";
    else if (spSid==21)
        location = "ml";
    else if (spSid==31)
        location = "nsf";
    else if (spSid==41)
        location = "nwsc";
    else if (spSid==51)
        location = "raf";
    else if (spSid==61)
        location = "fireside";
    else
        location = "mlw";
    //for adding a single line to influx db directly
    string data;

    //measurement name followed by the key name and key value in the influx db
    data = location + ",location=" + location + ",dsm_id=" + to_string(dsmId) + ",sensor_id=" + to_string(spSid) + " ";   
    for (unsigned int i = 0; i < nvalues; ++i)
    {
        double value = samp->getDataValue(i);
        if (std::isnan(value))
        {
//TO DO: should we have a boolean field set to true is the value is an NaN? write to db? or just continue?
            // cout << varnames[i] << " is " << value << "\n";
            continue;
        }
        else
        {
            //field name and field value in the influxdb
            if (i < nvalues-1){
                data += varnames[i] + "=" + to_string(value) + ",";
            }
            else{
                data += varnames[i] + "=" + to_string(value) + " " + to_string(samp->getTimeTag()) + "\n";
            }
        }
    }
    // cout << data << " TO BE WRITTEN TO INFLUXDB\n";

    //try synchronous insert into influxdb
    // cout << "...attempting to write to db with synchronous request\n";
    dataToInfluxDB(data);
    // cout << "...attempt to write to db with synchronous request complete\n";
}


class CounterClient: public SampleClient 
{
public:

    CounterClient(const list<DSMSensor*>& sensors, NidasApp& app);

    virtual ~CounterClient() {}

    void flush() throw() {}

    bool receive(const Sample* samp) throw();

    void printResults(std::ostream& outs);

    void
    printData(std::ostream& outs, SampleCounter& ss);

    void resetResults();

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

    typedef map<dsm_sample_id_t, SampleCounter> sample_map_t;

    /**
     * Find the SampleCounter for the given sample ID.  Wisard samples get
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

    NidasApp& _app;
};

void
CounterClient::
resetResults()
{
    //cout << "... CounterClient::resetResults\n";
    sample_map_t::iterator si;
    for (si = _samples.begin(); si != _samples.end(); ++si)
    {
        si->second.reset();
    }
}



CounterClient::CounterClient(const list<DSMSensor*>& sensors, NidasApp& app):
    _samples(),
    _reportall(false),
    _reportdata(false),
    _app(app)
{
    //cout << "... CounterClient::CounterClient\n";
    bool processed = app.processData();
    //cout << "... process data? " << processed << "\n";
    SampleMatcher& matcher = _app.sampleMatcher();
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si)
    {
        // Create a SampleCounter for samples from the given sensors.  Raw
        // samples are named by the sensor device, processed samples by the
        // first variable in the first sample tag.
        DSMSensor* sensor = *si;
        string sname = sensor->getDSMConfig()->getName() + ":" +
            sensor->getDeviceName();

        // Stop with raw samples if processed not requested.
        if (! processed)
        {
            if (matcher.match(sensor->getId()))
            {
                dsm_sample_id_t sid = sensor->getId();
                cout << "... DLOG:adding raw sample with sid: " << sid << " with sname:" << sname << "\n";
                DLOG(("adding raw sample: ") << _app.formatId(sid));
                SampleCounter stats(sid, sname);
                _samples[stats.id] = stats;
            }
            continue;
        }

	// for samples show the first variable name, followed by ",..."
	// if more than one.
	SampleTagIterator ti = sensor->getSampleTagIterator();
	for ( ; ti.hasNext(); ) {
	    const SampleTag* stag = ti.next();
        cout << "************************** " << stag->getSuffix() << " ***********\n";
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
                if (! matcher.match(sid))
                {
                    continue;
                }
                sample_map_t::iterator it = findStats(sid);
                if (it == _samples.end())
                {
                    cout << "... DLOG((adding processed sample: ) with _app.formatId(sid)): " << _app.formatId(sid) << "\n";
                    DLOG(("adding processed sample: ") << _app.formatId(sid));
                    SampleCounter pstats(sid, varname, stag);
                    _samples[pstats.id] = pstats;
                }
	    }
	}
    }
}

bool CounterClient::receive(const Sample* samp) throw()
{
    //cout << "... CounterClient::receive\n";
    dsm_sample_id_t sampid = samp->getId();
    if (! _app.sampleMatcher().match(sampid))
    {
        return false;
    }
    //cout << "... VLOG: received and accepted sample\n";
    VLOG(("received and accepted sample ") << _app.formatId(sampid));
    sample_map_t::iterator it = findStats(sampid);
    if (it == _samples.end())
    {
        // When there is no header from which to gather samples ahead of
        // time, just add a SampleCounter instance for any new raw sample
        // that arrives.
        DLOG(("creating counter for sample id ") << _app.formatId(sampid));
        SampleCounter ss(sampid);
        _samples[sampid] = ss;
        it = findStats(sampid);
    }
    //cout << "...fin\n";
    return it->second.receive(samp);
}


namespace
{
    /**
     * Compute the number of digits of space required to display
     * @p value in decimal.
     **/
    inline int
    ndigits(double value)
    {
        return (int)ceil(log10(value));
    }

    struct check_valid
    {
        double _value;
        bool _valid;

        check_valid(double value, bool valid) :
            _value(value),
            _valid(valid)
        {
        }            

        inline std::ostream&
        to_stream(std::ostream& outs) const
        {
            if (_valid)
            {
                outs << _value;
            }
            else
            {
                outs << floatNAN;
            }
            return outs;
        }
    };

    inline std::ostream&
    operator<<(std::ostream& outs, const check_valid& cv)
    {
        return cv.to_stream(outs);
    }
}



void CounterClient::printResults(std::ostream& outs)
{
    //cout << "... CounterClient::printResults\n";
    size_t maxnamelen = 6;
    int lenpow[2] = {5,5};
    int dtlog10[2] = {7,7};

    sample_map_t::iterator si;
    for (si = _samples.begin(); si != _samples.end(); ++si)
    {
        SampleCounter &ss = si->second;
        if (ss.nsamps == 0 && !_reportall)
            continue;

        maxnamelen = std::max(maxnamelen, ss.name.length());
        if (ss.nsamps >= 1)
        {
            lenpow[0] = std::max(lenpow[0], ndigits(ss.minlens)+1);
            lenpow[1] = std::max(lenpow[1], ndigits(ss.maxlens)+1);
        }
        // Skip min/max stats which will be printed as missing if there are
        // not at least two samples.
        if (ss.nsamps >= 2)
        {
            int dt = abs(ss.minDeltaTs);
            dtlog10[0] = std::max(dtlog10[0], ndigits(dt+1)+2);
            dt = ss.maxDeltaTs;
            dtlog10[1] = std::max(dtlog10[1], ndigits(dt+1)+2);
        }
    }
        
    struct tm tm;
    char tstr[64];
    cout << "\n**************************************************\n";
    cout << "************** TEST intern BARNITZ ***************\n";
    cout << "**************************************************\n";
    outs << left << setw(maxnamelen) << (maxnamelen > 0 ? "sensor" : "")
         << right
         << "  dsm sampid    nsamps |------- start -------|  |------ end -----|"
         << "    rate"
         << setw(dtlog10[0] + dtlog10[1]) << " minMaxDT(sec)"
         << setw(lenpow[0] + lenpow[1]) << " minMaxLen"
         << endl;

    for (si = _samples.begin(); si != _samples.end(); ++si)
    {
        cout << "&&&&&&&&&&&&&\n";
        SampleCounter& ss = si->second;
        if (ss.nsamps == 0 && !_reportall)
            continue;

	string t1str;
	string t2str;
        if (ss.nsamps > 0)
        {
            time_t ut = ss.t1s / USECS_PER_SEC;
            gmtime_r(&ut,&tm);
            strftime(tstr,sizeof(tstr),"%Y %m %d %H:%M:%S",&tm);
            int msec = (int)(ss.t1s % USECS_PER_SEC) / USECS_PER_MSEC;
            sprintf(tstr + strlen(tstr),".%03d",msec);
            t1str = tstr;
            ut = ss.t2s / USECS_PER_SEC;
            gmtime_r(&ut,&tm);
            strftime(tstr,sizeof(tstr),"%m %d %H:%M:%S",&tm);
            msec = (int)(ss.t2s % USECS_PER_SEC) / USECS_PER_MSEC;
            sprintf(tstr + strlen(tstr),".%03d",msec);
            t2str = tstr;
        }
        else
        {
            t1str = string((size_t)23, '*');
            t2str = string((size_t)18, '*');
        }

        outs << left << setw(maxnamelen) << ss.name
             << right << ' ' << setw(4) << GET_DSM_ID(ss.id) << ' ';

        NidasApp* app = NidasApp::getApplicationInstance();
        app->formatSampleId(outs, ss.id);

        double rate = double(ss.nsamps-1) /
            (double(ss.t2s - ss.t1s) / USECS_PER_SEC);
        outs << setw(9) << ss.nsamps << ' '
             << t1str << "  " << t2str << ' '
             << fixed << setw(7) << setprecision(2)
             << check_valid(rate, bool(ss.nsamps > 1))
             << setw(dtlog10[0]) << setprecision(3)
             << check_valid((double)ss.minDeltaTs / MSECS_PER_SEC, (ss.nsamps > 1))
             << setw(dtlog10[1]) << setprecision(3)
             << check_valid((float)ss.maxDeltaTs / MSECS_PER_SEC, (ss.nsamps > 1))
             << setprecision(0)
             << setw(lenpow[0]) << check_valid(ss.minlens, (ss.nsamps > 0))
             << setw(lenpow[1]) << check_valid(ss.maxlens, (ss.nsamps > 0))
             << endl;

        if (_reportdata)
            printData(outs, ss);
    }
}


void
CounterClient::
printData(std::ostream& outs, SampleCounter& ss)
{
    cout << "... CounterClient::printData\n";
    if (ss.rawmsg.length())
    {
        outs << " raw msg " << ss.rawmsg << endl;
    }
    if (ss.sums.size() == 0)
    {
        return;
    }
    size_t nwidth = 8;
    outs.unsetf(std::ios::fixed);
    outs << setprecision(3) << fixed;

    size_t maxname = 0;
    for (unsigned int i = 0; i < ss.varnames.size(); ++i)
    {
        maxname = std::max(maxname, ss.varnames[i].length());
    }
    int nfields = std::max((size_t)2, 80 / (maxname+2+nwidth));

    for (unsigned int i = 0; i < ss.sums.size(); ++i)
    {
        if (i > 0 && i % nfields == 0)
        {
            outs << endl;
        }
        string varname;
        if (i < ss.varnames.size())
        {
            //cout << "\n i: " << i << "\n";
            varname = ss.varnames[i];
        }
        outs << " **" << setw(maxname) << right << varname << "=" << left;
        outs << setw(nwidth);
        int nvalues = ss.nsamps - ss.nnans[i];
        // cout << "\nnvalues: " << nvalues << ", ss.nsamps: " << ss.nsamps << " ss.nnans[i]: " << ss.nnans[i] << "\n";
        if (nvalues == 0)
        {
            outs << string(nwidth, '*');
        }
        else
        {
            outs << ss.sums[i]/nvalues;
        }
        if (nvalues && ss.nnans[i] > 0)
        {
            outs << "(*" << ss.nnans[i] << " NaN*)";
        }
    }
    outs << endl;
}



class DataStats
{
public:
    DataStats();

    ~DataStats() {}

    int run() throw();

    void readHeader(SampleInputStream& sis);

    void readSamples(SampleInputStream& sis);

    int parseRunstring(int argc, char** argv);

    static int main(int argc, char** argv);

    int usage(const char* argv0);

    bool
    reportsExhausted(int nreports=-1)
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


bool DataStats::_alarm(false);


void
DataStats::handleSignal(int signum)
{
    // The NidasApp handler sets interrupted before calling this handler,
    // so clear that if this is just the interval alarm.
    if (signum == SIGALRM)
    {
        NidasApp::setInterrupted(false);
        _alarm = true;
    }
}


DataStats::DataStats():
    _realtime(false), _period_start(time_t(0)),
    _count(1), _period(0), _nreports(0),
    app("data_stats"),
    Period("-P,--period", "<seconds>",
           "Collect statistics for the given number of seconds and then "
           "print the report.\n"
           "If 0, wait until interrupted with Ctl-C.", "0"),
    Count("-n,--count", "<count>",
          "When --period specified, generate <count> reports.\n"
          "Use a count of zero to continue reports until interrupted.", "1"),
    AllSamples("-a,--all", "",
               "Show statistics for all sample IDs, including those for which "
               "no samples are received."),
    ShowData("-D,--data", "",
             "Print data for each sensor, either the last received message\n"
             "for raw samples, or data values averaged over the recording\n"
             "period for processed samples.")
{
    //cout << "... DataStats::DataStats\n";
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


int DataStats::parseRunstring(int argc, char** argv)
{
    //cout << "... DataStats::parseRunString\n";
    // Setup a default log scheme which will get replaced if any logging is
    // configured on the command line.
    n_u::Logger* logger = n_u::Logger::getInstance();
    n_u::LogConfig lc("notice");
    logger->setScheme(logger->getScheme("default").addConfig(lc));

    try {
        ArgVector args = app.parseArgs(argc, argv);
        cout << "...try of DataStats:: parseRunString\n";
        if (app.helpRequested())
        {
            return usage(argv[0]);
        }
        _period = Period.asInt();
        _count = Count.asInt();

        app.parseInputs(args);
        //set process data to true? update the default to true for this appilcation
        //app._processData = true;
    }
    catch (NidasAppException& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

int DataStats::usage(const char* argv0)
{
    cout << "... DataStats::usage\n";
    cerr <<
        "Usage: " << argv0 << " [options] [inputURL] ...\n";
    cerr <<
        "Standard options:\n"
         << app.usage() <<
        "Examples:\n" <<
        argv0 << " xxx.dat yyy.dat\n" <<
        argv0 << " file:/tmp/xxx.dat file:/tmp/yyy.dat\n" <<
        argv0 << " -p -x ads3.xml sock:hyper:30000\n" << endl;
    return 1;
}

int DataStats::main(int argc, char** argv)
{
    DataStats stats;
    int result;
    if ((result = stats.parseRunstring(argc, argv)))
    {
        cout << "main if statement\n";
        return result;
    }
    cout << "main not if statement\n";
    return stats.run();
}

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};


void
DataStats::
readHeader(SampleInputStream& sis)
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
        try {
            sis.readInputHeader();
            header_read = true;
            // Reading the header does not count as a report cycle.
            --_nreports;
        }
        catch (n_u::IOException& e)
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


void
DataStats::
readSamples(SampleInputStream& sis)
{
    cout << "...DataStats::readSamples\n";
    // Read samples until an alarm signals the end of a reporting period or
    // an interruption occurs.
    _alarm = false;
    if (_period > 0 && _realtime)
    {
        alarm(_period);
    }
    while (!_alarm && !app.interrupted())
    {
        try {
            sis.readSamples();
        }
        catch (n_u::IOException& e)
        {
            DLOG(("") << e.what() << " (errno=" << e.getErrno() << ")");
            if (e.getErrno() != ERESTART && e.getErrno() != EINTR)
                throw;
        }
    }
}

//both dataStats and dataDump use similar run() 
int DataStats::run() throw()
{
    int result = 0;
    //cout << "...DataStats::run\n";
    try {
        //cout << "...try\n";
        AutoProject aproject;
	IOChannel* iochan = 0;

	if (app.dataFileNames().size() > 0)
        {
            //cout << "... if there is a dataFileName\n";
            nidas::core::FileSet* fset =
                nidas::core::FileSet::getFileSet(app.dataFileNames());
            iochan = fset->connect();
	}
	else
        {
	    n_u::Socket* sock = new n_u::Socket(*app.socketAddress());
	    iochan = new nidas::core::Socket(sock);
            _realtime = true; //in data stats not in data dump //necessary?
	}

	SampleInputStream sis(iochan, app.processData());
        sis.setMaxSampleLength(32768);
	// sis.init();

        if (_period > 0 && _realtime)
        {
            //cout << "... if _period >0 && _realtime\n";
            app.addSignal(SIGALRM, &DataStats::handleSignal, true);
        }
        readHeader(sis);

	const SampleInputHeader& header = sis.getInputHeader();

	list<DSMSensor*> allsensors;

        string xmlFileName = app.xmlHeaderFile();
        //cout << "... xmlFileName: *" << xmlFileName << "*\n";
    //if the file name is not given it will automatically be assigned to ****/weather-eol-stations.xml
	if (xmlFileName.length() == 0){
	    xmlFileName = header.getConfigName();
        cout << "xmlFileName.length == 0, fileName: " << xmlFileName <<"\n";
    }
    xmlFileName = n_u::Process::expandEnvVars(xmlFileName);
//TO DO: change this hard coding of file name //used because of trying to bypass giving CLA for processing data
    xmlFileName = "/h/eol/jbarnitz/barnitzSUPER/eol-weather-stations-TEMP2.xml"; //...change this
    //cout << "xmlFileName.length == 0, fileName: " << xmlFileName <<"\n";
    //cout << "... app.processData(): " << app.processData() << "\n";
	struct stat statbuf;
    //we should want to process all data that comes in to store in the DB as "raw" but processed data s.t. it is no longer ascii
	//thus use the .xml file for this 
    if (1 || ::stat(xmlFileName.c_str(), &statbuf) == 0 || app.processData())
        {
            //cout << "... if near 911\n";
            n_u::auto_ptr<xercesc::DOMDocument>
                doc(parseXMLConfigFile(xmlFileName));

	    Project::getInstance()->fromDOMElement(doc->getDocumentElement());

            DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
	    for ( ; di.hasNext(); )
            {
		const DSMConfig* dsm = di.next();
		const list<DSMSensor*>& sensors = dsm->getSensors();
		allsensors.insert(allsensors.end(),sensors.begin(),sensors.end());
	    }
	}
        XMLImplementation::terminate();

	SamplePipeline pipeline;                                  
        CounterClient counter(allsensors, app); //not in data_dump
        counter.reportAll(AllSamples.asBool());
        counter.reportData(ShowData.asBool());

	if (1 || app.processData()) {
        //cout << "... app.processData() is true\n";
            pipeline.setRealTime(false);                              
            pipeline.setRawSorterLength(0);                           
            pipeline.setProcSorterLength(0);                          

	    list<DSMSensor*>::const_iterator si;
	    for (si = allsensors.begin(); si != allsensors.end(); ++si) {
		DSMSensor* sensor = *si;
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
        else sis.addSampleClient(&counter);

        try {
            if (_period > 0 && _realtime)
            {
                cout << "....... Collecting samples for " << _period << " seconds "
                     << "......." << endl;
            }
            while (!app.interrupted() && !reportsExhausted(++_nreports))
            {
                readSamples(sis);
                counter.printResults(cout);
                counter.resetResults();
            }
        }
        catch (n_u::EOFException& e)
        {
            cerr << e.what() << endl;
            counter.printResults(cout);
        }
        catch (n_u::IOException& e)
        {
            if (1 || app.processData())
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
            counter.printResults(cout);
            throw(e);
        }
	if (1 || app.processData())
        {
            pipeline.disconnect(&sis);
            pipeline.flush();
            pipeline.getProcessedSampleSource()->removeSampleClient(&counter);
            pipeline.getRawSampleSource()->removeSampleClient(&counter);//maybe???
        }
        else
        {
            sis.removeSampleClient(&counter);
        }
        sis.close();
        pipeline.interrupt();
        pipeline.join();
    }
    catch (n_u::Exception& e) {
        cout << "exception\n";
        cerr << e.what() << endl;
        XMLImplementation::terminate(); // ok to terminate() twice
	result = 1;
    }
    return result;
}

int main(int argc, char** argv)
{
//TO DO: is this the best place for invoking the creation of a new database? potential for using CLA for assigning the DB a name, or obtaining that info from the sensor?
    createInfluxDB();
    return DataStats::main(argc, argv);
}
