/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#define _XOPEN_SOURCE	/* glibc2 needs this */
#include <time.h>

#include <FileSet.h>
#include <Socket.h>
#include <RawSampleInputStream.h>
#include <DSMEngine.h>
#include <SampleInputHeader.h>
#include <IRIGSensor.h>

#ifdef NEEDED
// #include <Sample.h>
#include <dsm_sample.h>
#include <atdUtil/EOFException.h>
#endif

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace dsm;
using namespace std;

class DumpClient: public SampleClient 
{
public:

    typedef enum format { DEFAULT, ASCII, HEX, SIGNED_SHORT, UNSIGNED_SHORT,
    	FLOAT, IRIG } format_t;

    DumpClient(dsm_sample_id_t,format_t,ostream&);

    virtual ~DumpClient() {}

    bool receive(const Sample* samp) throw();

    void printHeader();

private:
    dsm_sample_id_t sampleId;
    format_t format;
    ostream& ostr;

};


DumpClient::DumpClient(dsm_sample_id_t id,format_t fmt,ostream &outstr):
	sampleId(id),format(fmt),ostr(outstr)
{
}

void DumpClient::printHeader()
{
    cout << "|--- date time -------| deltaT   bytes" << endl;
}

bool DumpClient::receive(const Sample* samp) throw()
{
    dsm_time_t tt = samp->getTimeTag();
    static dsm_time_t prev_tt = 0;

    dsm_sample_id_t sampid = samp->getId();
    if (sampid != sampleId) return false;

    struct tm tm;
    char cstr[64];
    time_t ut = tt / USECS_PER_SEC;
    gmtime_r(&ut,&tm);
    int msec = (tt % USECS_PER_SEC) / USECS_PER_MSEC;
    strftime(cstr,sizeof(cstr),"%Y %m %d %H:%M:%S",&tm);
    ostr << cstr << '.' << setw(3) << setfill('0') << msec << ' ';
    ostr << setw(3) << (tt - prev_tt) / 1000 << ' ';
    ostr << setw(7) << setfill(' ') << samp->getDataByteLength() << ' ';
    prev_tt = tt;

    switch(format) {
    case ASCII:
	{
	string dstr((const char*)samp->getConstVoidDataPtr(),
		samp->getDataByteLength());
        ostr << dstr << endl;
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
	for (unsigned int i = 0; i < samp->getDataByteLength()/2; i++)
	    ostr << setw(6) << sp[i] << ' ';
	ostr << endl;
	}
        break;
    case UNSIGNED_SHORT:
	{
	const unsigned short* sp =
		(const unsigned short*) samp->getConstVoidDataPtr();
	ostr << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/2; i++)
	    ostr << setw(6) << sp[i] << ' ';
	ostr << endl;
	}
        break;
    case FLOAT:
	{
	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	ostr << setprecision(4) << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/4; i++)
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

private:
    static bool interrupted;

    bool processData;

    string xmlFileName;

    list<string> dataFileNames;

    string hostName;

    int port;

    dsm_sample_id_t sampleId;

    DumpClient::format_t format;

};

DataDump::DataDump(): processData(false),
	port(30000),sampleId(0),
	format(DumpClient::DEFAULT)
{
}

int DataDump::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "Ad:FHIps:SUx:")) != -1) {
	switch (opt_char) {
	case 'A':
	    format = DumpClient::ASCII;
	    break;
	case 'd':
	    sampleId = SET_DSM_ID(sampleId,atoi(optarg));
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
	case 'p':
	    processData = true;
	    break;
	case 's':
	    sampleId = SET_SHORT_ID(sampleId,atoi(optarg));
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
	    hostName = url.substr(0,ic);
	    if (ic < string::npos) {
		istringstream ist(url.substr(ic+1));
		ist >> port;
		if (ist.fail()) {
		    cerr << "Invalid port number: " << url.substr(ic+1) << endl;
		    return usage(argv[0]);
		}
	    }
	}
	else if (url.length() > 5 && !url.compare(0,5,"file:")) {
	    url = url.substr(5);
	    dataFileNames.push_back(url);
	}
	else dataFileNames.push_back(url);
    }
    if (dataFileNames.size() == 0 && hostName.length() == 0) return usage(argv[0]);

    if (sampleId < 0) return usage(argv[0]);
    return 0;
}

int DataDump::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " -d dsmid -s sampleId [-p] -x xml_file [-A | -H | -S] inputURL ...\n\
    -d dsmid: numeric id of DSM that you want to dump samples from\n\
    -s sampleId: numeric id of sample that you want to dump\n\
	(use data_stats program to see DSM ids and sample ids of data in a file)\n\
    -p: process (optional). Pass samples to sensor process method\n\
    -x xml_file (optional), default: \n\
         $ADS3_CONFIG/projects/<project>/<aircraft>/flights/<flight>/ads3.xml\n\
         where <project>, <aircraft> and <flight> are read from the input data header\n\
    -A: ASCII output (for samples from a serial sensor)\n\
    -F: floating point output (default for processed output)\n\
    -H: hex output (default for raw output)\n\
    -I: output of IRIG clock samples\n\
    -S: signed short output (useful for samples from an A2D)\n\
    inputURL: data input (required). Either \"sock:host:port\" or\n\
         one or more \"file:file_path\" or simply one or more file paths.\n\
Examples:\n" <<
	argv0 << " -d 0 -s 100 xxx.dat\n" <<
	argv0 << " -d 0 -s 100 xxx.dat yyy.dat\n" <<
	argv0 << " -d 0 -s 200 -p -x ads3.xml sock:hyper:30000\n" << endl;
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

    return dump.run();
}

int DataDump::run() throw()
{

    try {
	IOChannel* iochan = 0;

	if (dataFileNames.size() > 0) {
	    dsm::FileSet* fset = new dsm::FileSet();
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
	    atdUtil::Socket* sock = new atdUtil::Socket(hostName,port);
	    iochan = new dsm::Socket(sock);
	}

	RawSampleInputStream sis(iochan);	// RawSampleStream now owns the iochan ptr.
	sis.init();
	sis.readHeader();
	SampleInputHeader header = sis.getHeader();


	auto_ptr<Project> project;
	list<DSMSensor*> allsensors;

	if (xmlFileName.length() == 0) {
	    if (getenv("ISFF") != 0)
		xmlFileName = Project::getConfigName("$ISFF",
		    "projects", header.getProjectName(),
		    header.getSiteName(),"ops",
		    header.getObsPeriodName(),"ads3.xml");
	    else
	    	xmlFileName = Project::getConfigName("$ADS3_CONFIG",
		    "projects",header.getProjectName(),
		    header.getSiteName(),"flights",
		    header.getObsPeriodName(),"ads3.xml");
	}

	struct stat statbuf;
	if (::stat(xmlFileName.c_str(),&statbuf) == 0 || processData) {
	    auto_ptr<xercesc::DOMDocument> doc(
		DSMEngine::parseXMLConfigFile(xmlFileName));

	    project = auto_ptr<Project>(Project::getInstance());
	    project->fromDOMElement(doc->getDocumentElement());
	    const list<Site*>& sitelist = project->getSites();
	    list<Site*>::const_iterator ai;
	    for (ai = sitelist.begin(); ai != sitelist.end(); ++ai) {
		Site* site = *ai;
		const list<DSMConfig*>& dsms = site->getDSMConfigs();
		list<DSMConfig*>::const_iterator di;
		for (di = dsms.begin(); di != dsms.end(); ++di) {
		    DSMConfig* dsm = *di;
		    const list<DSMSensor*>& sensors = dsm->getSensors();
		    allsensors.insert(allsensors.end(),sensors.begin(),sensors.end());
		}
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
    catch (dsm::XMLException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (atdUtil::InvalidParameterException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (atdUtil::EOFException& e) {
	cerr << e.what() << endl;
    }
    catch (atdUtil::IOException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (atdUtil::Exception& e) {
	cerr << e.what() << endl;
	return 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    return DataDump::main(argc,argv);
}
