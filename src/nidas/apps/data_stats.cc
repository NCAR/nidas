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

#include <set>
#include <map>
#include <iostream>
#include <iomanip>
#include <sys/stat.h>

#include <unistd.h>
#include <getopt.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class CounterClient: public SampleClient 
{
public:

    CounterClient(const list<DSMSensor*>& sensors);

    virtual ~CounterClient() {}

    void flush() throw() {}

    bool receive(const Sample* samp) throw();

    void printResults();


private:
    map<dsm_sample_id_t,string> sensorNames;

    set<dsm_sample_id_t> sampids;

    map<dsm_sample_id_t,dsm_time_t> t1s;

    map<dsm_sample_id_t,dsm_time_t> t2s;

    map<dsm_sample_id_t,size_t> nsamps;

    map<dsm_sample_id_t,size_t> minlens;

    map<dsm_sample_id_t,size_t> maxlens;

    map<dsm_sample_id_t,int> minDeltaTs;

    map<dsm_sample_id_t,int> maxDeltaTs;
};

CounterClient::CounterClient(const list<DSMSensor*>& sensors):
    sensorNames(),sampids(),t1s(),t2s(),nsamps(),minlens(),maxlens(),
    minDeltaTs(),maxDeltaTs()
{
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
        DSMSensor* sensor = *si;
	sensorNames[sensor->getId()] =
	    sensor->getDSMConfig()->getName() + ":" + sensor->getDeviceName();

	// for samples show the first variable name, followed by ",..."
	// if more than one.
	SampleTagIterator ti = sensor->getSampleTagIterator();
	for ( ; ti.hasNext(); ) {
	    const SampleTag* stag = ti.next();
	    if (stag->getVariables().size() > 0) {
		string varname = stag->getVariables().front()->getName();
		if (stag->getVariables().size() > 1) varname += ",...";
		sensorNames[stag->getId()] = varname;
	    }
	}
    }
}

bool CounterClient::receive(const Sample* samp) throw()
{
    dsm_time_t sampt = samp->getTimeTag();

    dsm_sample_id_t sampid = samp->getId();
    sampids.insert(sampid);

    map<dsm_sample_id_t,dsm_time_t>::iterator t1i =
	t1s.find(sampid);
    if (t1i == t1s.end()) {
	t1s.insert(
	    make_pair(sampid,sampt));
	minDeltaTs[sampid] = INT_MAX;
    }
    else {
        int deltaT = (sampt - t2s[sampid] + USECS_PER_MSEC/2) / USECS_PER_MSEC;
	minDeltaTs[sampid] = std::min(minDeltaTs[sampid],deltaT);
	maxDeltaTs[sampid] = std::max(maxDeltaTs[sampid],deltaT);
    }
    t2s[sampid] = sampt;
    nsamps[sampid]++;

    size_t slen = samp->getDataByteLength();
    size_t mlen;

    map<dsm_sample_id_t,size_t>::iterator li = minlens.find(sampid);
    if (li == minlens.end()) minlens[sampid] = slen;
    else {
	mlen = li->second;
	if (slen < mlen) minlens[sampid] = slen;
    }

    mlen = maxlens[sampid];
    if (slen > mlen) maxlens[sampid] = slen;

    // cerr << samp->getDSMId() << ',' << samp->getSpSId() <<  " " << samp->getTimeTag() << endl;
    return true;
}

void CounterClient::printResults()
{
    size_t maxnamelen = 6;
    int lenpow[2] = {5,5};
    int dtlog10[2] = {7,7};
    set<dsm_sample_id_t>::iterator si;
    for (si = sampids.begin(); si != sampids.end(); ++si) {
	dsm_sample_id_t id = *si;
	const string& sname = sensorNames[id];
	if (sname.length() > maxnamelen) maxnamelen = sname.length();
	size_t m = minlens[id];
	if (m > 0) {
	    int p = (int)ceil(log10((double)m));
	    lenpow[0] = std::max(lenpow[0],p+1);
	}
	m = maxlens[id];
	if (m > 0) {
	    int p = (int)ceil(log10((double)m));
	    lenpow[1] = std::max(lenpow[1],p+1);
	}
	int dt = abs(minDeltaTs[id]);
	if (dt > 0 && dt < INT_MAX) {
	    int p = (int)ceil(log10((double)dt+1));
	    dtlog10[0] = std::max(dtlog10[0],p + 2);
	}
	dt = maxDeltaTs[id];
	if (dt > 0) {
	    int p = (int)ceil(log10((double)dt+1));
	    dtlog10[1] = std::max(dtlog10[1],p + 2);
	}
    }
        
    struct tm tm;
    char tstr[64];
    cout << left << setw(maxnamelen) << (maxnamelen > 0 ? "sensor" : "") <<
    	right <<
    	"  dsm sampid    nsamps |------- start -------|  |------ end -----|    rate" <<
		setw(dtlog10[0] + dtlog10[1]) << " minMaxDT(sec)" <<
		setw(lenpow[0] + lenpow[1]) << " minMaxLen" <<
		endl;
    for (si = sampids.begin(); si != sampids.end(); ++si) {
	dsm_sample_id_t id = *si;
	time_t ut = t1s[id] / USECS_PER_SEC;
	gmtime_r(&ut,&tm);
	strftime(tstr,sizeof(tstr),"%Y %m %d %H:%M:%S",&tm);
	int msec = (int)(t1s[id] % USECS_PER_SEC) / USECS_PER_MSEC;
	sprintf(tstr + strlen(tstr),".%03d",msec);
	string t1str(tstr);
	ut = t2s[id] / USECS_PER_SEC;
	gmtime_r(&ut,&tm);
	strftime(tstr,sizeof(tstr),"%m %d %H:%M:%S",&tm);
	msec = (int)(t2s[id] % USECS_PER_SEC) / USECS_PER_MSEC;
	sprintf(tstr + strlen(tstr),".%03d",msec);
	string t2str(tstr);


        cout << left << setw(maxnamelen) << sensorNames[id] << right << ' ' <<
	    setw(4) << GET_DSM_ID(id) << ' ';

        NidasApp* app = NidasApp::getApplicationInstance();
        app->formatSampleId(cout, id);

        cout << setw(9) << nsamps[id] << ' ' <<
	    t1str << "  " << t2str << ' ' << 
	    fixed << setw(7) << setprecision(2) <<
	    double(nsamps[id]-1) / (double(t2s[id]-t1s[id]) / USECS_PER_SEC) <<
	    setw(dtlog10[0]) << setprecision(3) <<
	    (minDeltaTs[id] < INT_MAX ? (float)minDeltaTs[id] / MSECS_PER_SEC : 0) <<
	    setw(dtlog10[1]) << setprecision(3) <<
	    (float)maxDeltaTs[id] / MSECS_PER_SEC <<
	    setw(lenpow[0]) << minlens[id] << setw(lenpow[1]) << maxlens[id] <<
	    endl;
    }
}

class DataStats
{
public:
    DataStats();

    ~DataStats() {}

    int run() throw();

    int parseRunstring(int argc, char** argv);

    static int main(int argc, char** argv);

    int usage(const char* argv0);

private:
    static const int DEFAULT_PORT = 30000;

    int _count;
    int _period;

    NidasApp app;
    NidasAppArg Period;
    NidasAppArg Count;
};


DataStats::DataStats():
    _count(0), _period(0),
    app("data_stats"),
    Period("--period", "<seconds>",
           "Collect statistics for the given number of seconds and then "
           "print the report.", "0"),
    Count("-n,--count", "<count>",
          "When --period specified, generate <count> reports.", "0")
{
    app.setApplicationInstance();
    app.setupSignals();
    app.enableArguments(app.XmlHeaderFile | app.LogConfig |
                        app.SampleRanges | app.FormatHexId |
                        app.FormatSampleId | app.ProcessData |
                        app.Version | app.InputFiles |
                        app.Help | Period | Count);
    app.InputFiles.allowFiles = true;
    app.InputFiles.allowSockets = true;
    app.InputFiles.setDefaultInput("sock:localhost", DEFAULT_PORT);
}


int DataStats::parseRunstring(int argc, char** argv)
{
    try {
        ArgVector args(argv+1, argv+argc);
        app.parseArguments(args);
        if (app.helpRequested())
        {
            return usage(argv[0]);
        }
        _period = Period.asInt();
        _count = Count.asInt();

        app.parseInputs(args);
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
        return result;
    }
    NidasApp::setupSignals();

    return stats.run();
}

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

int DataStats::run() throw()
{

    int result = 0;

    try {
        AutoProject aproject;

	IOChannel* iochan = 0;

	if (app.dataFileNames().size() > 0)
        {
            nidas::core::FileSet* fset =
                nidas::core::FileSet::getFileSet(app.dataFileNames());
            iochan = fset->connect();
	}
	else
        {
	    n_u::Socket* sock = new n_u::Socket(*app.socketAddress());
	    iochan = new nidas::core::Socket(sock);
	}

        // Start an alarm here, since the header is not sent until there's
        // a sample to send, so if there are no samples we could block
        // right here reading the header and never get to the readSamples()
        // loop.
        if (_period > 0)
        {
            app.addSignal(SIGALRM);
            alarm(_period);
        }

	SampleInputStream sis(iochan, app.processData());
        sis.setMaxSampleLength(32768);
	// sis.init();
	sis.readInputHeader();

	const SampleInputHeader& header = sis.getInputHeader();

	list<DSMSensor*> allsensors;

        string xmlFileName = app.xmlHeaderFile();
	if (xmlFileName.length() == 0)
	    xmlFileName = header.getConfigName();
	xmlFileName = n_u::Process::expandEnvVars(xmlFileName);

	struct stat statbuf;
	if (::stat(xmlFileName.c_str(), &statbuf) == 0 || app.processData())
        {
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
        CounterClient counter(allsensors);

	if (app.processData()) {
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
        }
        else sis.addSampleClient(&counter);

        try {
            for (;;) {
                sis.readSamples();
                if (app.interrupted()) break;
            }
        }
        catch (n_u::EOFException& e) {
            cerr << e.what() << endl;
        }
        catch (n_u::IOException& e) {
            if (app.processData()) {
                pipeline.getProcessedSampleSource()->removeSampleClient(&counter);
                pipeline.disconnect(&sis);
                pipeline.interrupt();
                pipeline.join();
            }
            else sis.removeSampleClient(&counter);
            sis.close();
            counter.printResults();
            throw(e);
        }
	if (app.processData()) {
            pipeline.disconnect(&sis);
            pipeline.flush();
            pipeline.getProcessedSampleSource()->removeSampleClient(&counter);
        }
        else sis.removeSampleClient(&counter);

        sis.close();
        pipeline.interrupt();
        pipeline.join();
        counter.printResults();
    }
    catch (n_u::Exception& e) {
        cerr << e.what() << endl;
        XMLImplementation::terminate(); // ok to terminate() twice
	result = 1;
    }
    return result;
}

int main(int argc, char** argv)
{
    return DataStats::main(argc, argv);
}
