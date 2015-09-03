/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
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
#include <nidas/util/EOFException.h>
#include <nidas/util/Process.h>
#include <nidas/util/Logger.h>

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

    CounterClient(const list<DSMSensor*>& sensors,bool hexIds);

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

    bool _hexIds;
};

CounterClient::CounterClient(const list<DSMSensor*>& sensors, bool hexIds):
    sensorNames(),sampids(),t1s(),t2s(),nsamps(),minlens(),maxlens(),
    minDeltaTs(),maxDeltaTs(), _hexIds(hexIds)
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
	    make_pair<dsm_sample_id_t,dsm_time_t>(sampid,sampt));
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

        if (_hexIds) cout << "0x" << setw(4) << setfill('0') << hex <<
            GET_SPS_ID(id) << setfill(' ') << dec << ' ';
        else cout << setw(6) << GET_SPS_ID(id) << ' ';

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

    static int usage(const char* argv0);

    static void sigAction(int sig, siginfo_t* siginfo, void*);

    static void setupSignals();

    int logLevel;

private:
    static bool interrupted;

    static const int DEFAULT_PORT = 30000;

    bool processData;

    string xmlFileName;

    list<string> dataFileNames;

    auto_ptr<n_u::SocketAddress> sockAddr;

    bool hexIds;

};

bool DataStats::interrupted = false;

void DataStats::sigAction(int sig, siginfo_t* siginfo, void*) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            DataStats::interrupted = true;
    break;
    }
}

void DataStats::setupSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGINT);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);
                                                                                
    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = DataStats::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

DataStats::DataStats():
    logLevel(n_u::LOGGER_NOTICE),
    processData(false),xmlFileName(),dataFileNames(),
    sockAddr(0), hexIds(false)
{
}

int DataStats::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
										
    while ((opt_char = getopt(argc, argv, "l:px:X")) != -1) {
	switch (opt_char) {
	case 'l':
            logLevel = atoi(optarg);
	    break;
	case 'p':
	    processData = true;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case 'X':
	    hexIds = true;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    for (; optind < argc; optind++) {
	string url(argv[optind]);
	if (url.length() > 5 && !url.compare(0,5,"sock:")) {
	    url = url.substr(5);
	    size_t ic = url.find(':');
	    string hostName = url.substr(0,ic);
            int port = DEFAULT_PORT;
	    if (ic < string::npos) {
		istringstream ist(url.substr(ic+1));
		ist >> port;
		if (ist.fail()) {
		    cerr << "Invalid port number: " << url.substr(ic+1) << endl;
		    return usage(argv[0]);
		}
	    }
            try {
                n_u::Inet4Address addr = n_u::Inet4Address::getByName(hostName);
                sockAddr.reset(new n_u::Inet4SocketAddress(addr,port));
            }
            catch(const n_u::UnknownHostException& e) {
                cerr << e.what() << endl;
                return usage(argv[0]);
            }
	}
	else if (url.length() > 5 && !url.compare(0,5,"unix:")) {
	    url = url.substr(5);
            sockAddr.reset(new n_u::UnixSocketAddress(url));
	}
	else dataFileNames.push_back(url);
    }
    if (dataFileNames.size() == 0 && !sockAddr.get()) {
        try {
	    string hostName("localhost");
            int port = DEFAULT_PORT;
            n_u::Inet4Address addr = n_u::Inet4Address::getByName(hostName);
            sockAddr.reset(new n_u::Inet4SocketAddress(addr,port));
        }
        catch(const n_u::UnknownHostException& e) {
            cerr << e.what() << endl;
            return usage(argv[0]);
        }
    }

    return 0;
}

int DataStats::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-l log_level] [-p] [-x xml_file] [inputURL] ...\n\
    -l log_level: 7=debug,6=info,5=notice,4=warn,3=err, default=5\n\
    -p: process (optional). Pass samples to sensor process method\n\
    -X: print sample ids in hex format\n\
    -x xml_file (optional), default: \n\
	 $ADS3_CONFIG/projects/<project>/<aircraft>/flights/<flight>/ads3.xml\n\
	 where <project>, <aircraft> and <flight> are read from the input data header\n\
    inputURL: data input. One of the following:\n\
        sock:host[:port]          (Default port is " << DEFAULT_PORT << ")\n\
        unix:sockpath             unix socket name\n\
        path                      one or more file names\n\
        The default URL is sock:localhost\n\
Examples:\n" <<
    argv0 << " xxx.dat yyy.dat\n" <<
    argv0 << " file:/tmp/xxx.dat file:/tmp/yyy.dat\n" <<
    argv0 << " -p -x ads3.xml sock:hyper:30000\n" << endl;
    return 1;
}

int DataStats::main(int argc, char** argv)
{

    DataStats stats;
    int result;
    if ((result = stats.parseRunstring(argc,argv))) return result;

    n_u::LogConfig lc;
    lc.level = stats.logLevel;
    n_u::Logger::getInstance()->setScheme(
        n_u::LogScheme().addConfig (lc));

    setupSignals();

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

	if (dataFileNames.size() > 0) {
            nidas::core::FileSet* fset =
                nidas::core::FileSet::getFileSet(dataFileNames);
            iochan = fset->connect();
	}
	else {
	    n_u::Socket* sock = new n_u::Socket(*sockAddr.get());
	    iochan = new nidas::core::Socket(sock);
	}

	SampleInputStream sis(iochan,processData);
        sis.setMaxSampleLength(32768);
	// sis.init();
	sis.readInputHeader();

	const SampleInputHeader& header = sis.getInputHeader();

	list<DSMSensor*> allsensors;

	if (xmlFileName.length() == 0)
	    xmlFileName = header.getConfigName();
	xmlFileName = n_u::Process::expandEnvVars(xmlFileName);

	struct stat statbuf;
	if (::stat(xmlFileName.c_str(),&statbuf) == 0 || processData) {

	    auto_ptr<xercesc::DOMDocument> doc(parseXMLConfigFile(xmlFileName));

	    Project::getInstance()->fromDOMElement(doc->getDocumentElement());

	    for ( DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
	    	di.hasNext(); ) {
		const DSMConfig* dsm = di.next();
		const list<DSMSensor*>& sensors = dsm->getSensors();
		allsensors.insert(allsensors.end(),sensors.begin(),sensors.end());
	    }
	}
        XMLImplementation::terminate();

	SamplePipeline pipeline;                                  
        CounterClient counter(allsensors,hexIds);

	if (processData) {
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
                if (interrupted) break;
            }
        }
        catch (n_u::EOFException& e) {
            cerr << e.what() << endl;
        }
        catch (n_u::IOException& e) {
            if (processData) {
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
	if (processData) {
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
    return DataStats::main(argc,argv);
}
