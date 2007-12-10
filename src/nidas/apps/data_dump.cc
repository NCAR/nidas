/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

// #define _XOPEN_SOURCE	/* glibc2 needs this */

#include <ctime>

#include <nidas/dynld/FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/SampleInputHeader.h>
#include <nidas/dynld/raf/IRIGSensor.h>
#include <nidas/util/Logger.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

class DumpClient: public SampleClient 
{
public:

    typedef enum format { DEFAULT, ASCII, HEX, SIGNED_SHORT, UNSIGNED_SHORT,
    	FLOAT, IRIG, LONG } format_t;

    DumpClient(dsm_sample_id_t,format_t,ostream&);

    virtual ~DumpClient() {}

    bool receive(const Sample* samp) throw();

    void printHeader();

private:

    dsm_sample_id_t dsmId;

    dsm_sample_id_t sensorId;

    bool allDSMs;

    bool allSensors;

    format_t format;

    ostream& ostr;
};


DumpClient::DumpClient(dsm_sample_id_t id,format_t fmt,ostream &outstr):
        dsmId(GET_DSM_ID(id)),sensorId(GET_SHORT_ID(id)),
        allDSMs(false),allSensors(false),
	format(fmt),ostr(outstr)
{
    if (dsmId == 1023) allDSMs = true;
    if (sensorId == 65535) allSensors = true;
}

void DumpClient::printHeader()
{
    cout << "|--- date time --------|  deltaT";
    if (allDSMs || allSensors) cout << "   id   ";
    cout << "     len bytes" << endl;
}

bool DumpClient::receive(const Sample* samp) throw()
{
    dsm_time_t tt = samp->getTimeTag();
    static dsm_time_t prev_tt = 0;

    dsm_sample_id_t sampid = samp->getId();

#ifdef DEBUG
    cerr << "sampid=" << GET_DSM_ID(sampid) << ',' <<
    	GET_SHORT_ID(sampid) << endl;
#endif

    if (!allDSMs && GET_DSM_ID(sampid) != dsmId) return false;
    if (!allSensors && GET_SHORT_ID(sampid) != sensorId) return false;

    struct tm tm;
    char cstr[64];
    time_t ut = tt / USECS_PER_SEC;
    gmtime_r(&ut,&tm);
    int tmsec = (tt % USECS_PER_SEC) / (USECS_PER_MSEC / 10);

    strftime(cstr,sizeof(cstr),"%Y %m %d %H:%M:%S",&tm);
    ostr << cstr << '.' << setw(4) << setfill('0') << tmsec << ' ';

    ostr << setprecision(4) << setfill(' ');
    if (prev_tt != 0) {
        double tdiff = (tt - prev_tt) / (double)(USECS_PER_SEC);
        ostr << setw(7) << tdiff << ' ';
    }
    else ostr << setw(7) << 0 << ' ';

    if (allDSMs || allSensors)
        ostr << setw(2) << setfill(' ') << GET_DSM_ID(sampid) <<
        ',' << setw(4) << GET_SHORT_ID(sampid) << ' ';

    ostr << setw(7) << setfill(' ') << samp->getDataByteLength() << ' ';
    prev_tt = tt;

    switch(format) {
    case ASCII:
	{
        const char* cp = (const char*)samp->getConstVoidDataPtr();
        size_t l = samp->getDataByteLength();
        if (l > 0 && cp[l-1] == '\0') l--;  // exclude trailing '\0'
        ostr << string(cp,l) << endl;
        }
        break;
    case HEX:
        {
	const unsigned char* cp =
		(const unsigned char*) samp->getConstVoidDataPtr();
	ostr << setfill('0');
	for (unsigned int i = 0; i < samp->getDataByteLength(); i++)
	    ostr << hex << setw(2) << (unsigned int)cp[i] << dec << ' ';
	ostr << endl;
	}
        break;
    case SIGNED_SHORT:
	{
	const short* sp =
		(const short*) samp->getConstVoidDataPtr();
	ostr << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/sizeof(short); i++)
	    ostr << setw(6) << sp[i] << ' ';
	ostr << endl;
	}
        break;
    case UNSIGNED_SHORT:
	{
	const unsigned short* sp =
		(const unsigned short*) samp->getConstVoidDataPtr();
	ostr << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/sizeof(short); i++)
	    ostr << setw(6) << sp[i] << ' ';
	ostr << endl;
	}
        break;
    case FLOAT:
	{
	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	ostr << setprecision(4) << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/sizeof(float); i++)
	    ostr << setw(10) << fp[i] << ' ';
	ostr << endl;
	}
        break;
    case IRIG:
	{
	const unsigned char* dp = (const unsigned char*) samp->getConstVoidDataPtr();
	struct timeval tv;
	memcpy(&tv,dp,sizeof(tv));
	dp += sizeof(tv);
	unsigned char status = *dp;
	char timestr[128];
	struct tm tm;
	gmtime_r(&tv.tv_sec,&tm);
	strftime(timestr,sizeof(timestr)-1,"%Y %m %d %H:%M:%S",&tm);

	ostr << timestr << '.' << setw(6) << setfill('0') << tv.tv_usec <<
		' ' << setw(2) << setfill('0') << hex << (int)status << dec <<
		'(' << IRIGSensor::statusString(status) << ')';
	ostr << endl;
	}
        break;
    case LONG:
	{
	const long* lp =
		(const long*) samp->getConstVoidDataPtr();
	ostr << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/sizeof(long); i++)
	    ostr << setw(8) << lp[i] << ' ';
	ostr << endl;
	}
        break;
    case DEFAULT:
        break;
    }
    return true;
}

class DataDump
{
public:

    DataDump();

    int parseRunstring(int argc, char** argv);

    int run() throw();

    static int main(int argc, char** argv);

    static int usage(const char* argv0);

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    int logLevel;

private:

    static const int DEFAULT_PORT = 30000;

    static bool interrupted;

    bool processData;

    string xmlFileName;

    list<string> dataFileNames;

    auto_ptr<n_u::SocketAddress> sockAddr;

    dsm_sample_id_t sampleId;

    DumpClient::format_t format;


};

DataDump::DataDump():
        logLevel(n_u::LOGGER_INFO),
        processData(false),
	sampleId(0),
	format(DumpClient::DEFAULT)
{
}

int DataDump::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "Ad:FHIl:Lps:SUx:")) != -1) {
	switch (opt_char) {
	case 'A':
	    format = DumpClient::ASCII;
	    break;
	case 'd':
            {
                int dsmid = atoi(optarg);
                if (dsmid < 0)
                    sampleId = SET_DSM_ID(sampleId,0xffffffff);
                else
                    sampleId = SET_DSM_ID(sampleId,dsmid);
            }
	    break;
	case 'F':
	    format = DumpClient::FLOAT;
	    break;
	case 'H':
	    format = DumpClient::HEX;
	    break;
	case 'I':
	    format = DumpClient::IRIG;
	    break;
	case 'l':
	    logLevel = atoi(optarg);
	    break;
	case 'L':
	    format = DumpClient::LONG;
	    break;
	case 'p':
	    processData = true;
	    break;
	case 's':
            {
                int sensorid = atoi(optarg);
                if (sensorid < 0)
                    sampleId = SET_SHORT_ID(sampleId,0xffffffff);
                else
                    sampleId = SET_SHORT_ID(sampleId,sensorid);
            }
	    break;
	case 'S':
	    format = DumpClient::SIGNED_SHORT;
	    break;
	case 'U':
	    format = DumpClient::UNSIGNED_SHORT;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (format == DumpClient::DEFAULT)
    	format = (processData ? DumpClient::FLOAT : DumpClient::HEX);

    for ( ; optind < argc; optind++) {
	string url(argv[optind]);
	if (url.length() > 5 && !url.compare(0,5,"sock:")) {
	    url = url.substr(5);
	    size_t ic = url.find(':');
            int port = DEFAULT_PORT;
            string hostName = url.substr(0,ic);
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
    if (dataFileNames.size() == 0 && !sockAddr.get()) return usage(argv[0]);

    if (sampleId == 0) return usage(argv[0]);
    return 0;
}

int DataDump::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " -d dsmid -s sampleId [-l log_level] [-p] [-x xml_file] [-A | -H | -S] inputURL ...\n\
    -d dsmid: numeric id of DSM that you want to dump samples from (-1 for all)\n\
    -s sampleId: numeric id of sample that you want to dump (-1 for all)\n\
	(use data_stats program to see DSM ids and sample ids of data in a file)\n\
    -p: process (optional). Pass samples to sensor process method\n\
    -x xml_file (optional), default: \n\
         $ADS3_CONFIG/projects/<project>/<aircraft>/flights/<flight>/ads3.xml\n\
         where <project>, <aircraft> and <flight> are read from the input data header\n\
    -A: ASCII output (for samples from a serial sensor)\n\
    -F: floating point output (default for processed output)\n\
    -H: hex output (default for raw output)\n\
    -I: output of IRIG clock samples\n\
    -L: signed long output\n\
    -l log_level: 7=debug,6=info,5=notice,4=warn,3=err, default=6\n\
    -S: signed short output (useful for samples from an A2D)\n\
    inputURL: data input (required). One of the following:\n\
        sock:host[:port]          (Default port is " << DEFAULT_PORT << ")\n\
        unix:sockpath             unix socket name\n\
        path                      one or more file names\n\
Examples:\n" <<
    argv0 << " -d 0 -s 100 xxx.dat\n" <<
    argv0 << " -d 0 -s 100 xxx.dat yyy.dat\n" <<
    argv0 << " -d 0 -s 200 -x ads3.xml sock:hyper:30000\n" <<
    argv0 << " -d 0 -s 201 -p sock:hyper:30000\n" <<
    argv0 << " -d 0 -s 201 -p unix:/tmp/dsm\n" << endl;
    return 1;
}
/* static */
bool DataDump::interrupted = false;

/* static */
void DataDump::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            DataDump::interrupted = true;
    break;
    }
}

/* static */
void DataDump::setupSignals()
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
    act.sa_sigaction = DataDump::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int DataDump::main(int argc, char** argv)
{
    setupSignals();

    DataDump dump;

    int res;

    if ((res = dump.parseRunstring(argc,argv))) return res;

    n_u::LogConfig lc;
    lc.level = dump.logLevel;
    n_u::Logger::getInstance()->setScheme(
        n_u::LogScheme().addConfig (lc));

    return dump.run();
}

int DataDump::run() throw()
{

    try {
	IOChannel* iochan = 0;

	if (dataFileNames.size() > 0) {
	    FileSet* fset = new nidas::dynld::FileSet();
	    iochan = fset;

#ifdef USE_FILESET_TIME_CAPABILITY
	    struct tm tm;
	    strptime("2005 04 05 00:00:00","%Y %m %d %H:%M:%S",&tm);
	    time_t start = timegm(&tm);

	    strptime("2005 04 06 00:00:00","%Y %m %d %H:%M:%S",&tm);
	    time_t end = timegm(&tm);

	    fset->setDir("/tmp/RICO/hiaper");
	    fset->setFileName("radome_%Y%m%d_%H%M%S.dat");
	    fset->setStartTime(start);
	    fset->setEndTime(end);
#else
	    list<string>::const_iterator fi;
	    for (fi = dataFileNames.begin(); fi != dataFileNames.end(); ++fi)
		  fset->addFileName(*fi);
#endif
	}
	else {
	    n_u::Socket* sock = new n_u::Socket(*sockAddr.get());
	    iochan = new nidas::core::Socket(sock);
	}

	RawSampleInputStream sis(iochan);	// RawSampleStream now owns the iochan ptr.
	sis.init();
	sis.readHeader();
	SampleInputHeader header = sis.getHeader();


	auto_ptr<Project> project;
	list<DSMSensor*> allsensors;

	if (xmlFileName.length() == 0)
	    xmlFileName = header.getConfigName();
	xmlFileName = Project::expandEnvVars(xmlFileName);

	struct stat statbuf;
	if (::stat(xmlFileName.c_str(),&statbuf) == 0 || processData) {
	    auto_ptr<xercesc::DOMDocument> doc(
		DSMEngine::parseXMLConfigFile(xmlFileName));

	    project = auto_ptr<Project>(Project::getInstance());
	    project->fromDOMElement(doc->getDocumentElement());

	    DSMConfigIterator di = project->getDSMConfigIterator();

	    for ( ; di.hasNext(); ) {
		const DSMConfig* dsm = di.next();
		const list<DSMSensor*>& sensors = dsm->getSensors();
		allsensors.insert(allsensors.end(),sensors.begin(),
			sensors.end());
	    }
	}

	DumpClient dumper(sampleId,format,cout);

	if (processData) {
	    list<DSMSensor*>::const_iterator si;
	    for (si = allsensors.begin(); si != allsensors.end(); ++si) {
		DSMSensor* sensor = *si;
		sensor->init();
		sis.addProcessedSampleClient(&dumper,sensor);
	    }
	}
	else sis.addSampleClient(&dumper);

	dumper.printHeader();

	for (;;) {
	    sis.readSamples();
	    if (interrupted) break;
	}
    }
    catch (nidas::core::XMLException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::InvalidParameterException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::EOFException& e) {
	cerr << e.what() << endl;
    }
    catch (n_u::IOException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::Exception& e) {
	cerr << e.what() << endl;
	return 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    return DataDump::main(argc,argv);
}
