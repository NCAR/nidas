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

    typedef enum format { DEFAULT, ASCII, HEX, SIGNED_SHORT, FLOAT } format_t;

    DumpClient(dsm_sample_id_t,format_t,ostream&);

    virtual ~DumpClient() {}

    bool receive(const Sample* samp) throw();

    void printHeader();

private:
    dsm_sample_id_t sampleId;
    format_t format;
    ostream& ostr;

};

class Runstring {
public:
    Runstring(int argc, char** argv);

    static void usage(const char* argv0);
    bool process;
    string xmlFileName;
    string dataFileName;
    string hostName;
    int port;
    dsm_sample_id_t sampleId;
    DumpClient::format_t format;
};

Runstring::Runstring(int argc, char** argv): process(false),
	port(30000),sampleId(0),
	format(DumpClient::DEFAULT)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    xmlFileName = DSMServer::getADS3ConfigDir() + "/ads3.xml";
										
    while ((opt_char = getopt(argc, argv, "Ad:FHps:Sx:")) != -1) {
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
	case 'p':
	    process = true;
	    break;
	case 's':
	    sampleId = SET_SHORT_ID(sampleId,atoi(optarg));
	    break;
	case 'S':
	    format = DumpClient::SIGNED_SHORT;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }
    if (format == DumpClient::DEFAULT)
    	format = (process ? DumpClient::FLOAT : DumpClient::HEX);

    if (optind == argc - 1) {
	string url(argv[optind++]);
	if (url.length() > 5 && !url.compare(0,5,"sock:")) {
	    url = url.substr(5);
	    size_t ic = url.find(':');
	    hostName = url.substr(0,ic);
	    if (ic < string::npos) {
		istringstream ist(url.substr(ic+1));
		ist >> port;
		if (ist.fail()) {
		    cerr << "Invalid port number: " << url.substr(ic+1) << endl;
		    usage(argv[0]);
		}
	    }
	}
	else if (url.length() > 5 && !url.compare(0,5,"file:")) {
	    url = url.substr(5);
	    dataFileName = url;
	}
	else dataFileName = url;
    }
    if (dataFileName.length() == 0 && hostName.length() == 0) usage(argv[0]);
    if (process && xmlFileName.length() == 0) usage(argv[0]);

    if (sampleId < 0) usage(argv[0]);
    if (optind != argc) usage(argv[0]);
}

void Runstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-p] -d dsmid -s sampleId -x xml_file URL [-A | -H | -S]\n\
  -d dsmid: numeric id of DSM that you want to dump samples from\n\
  -s sampleId: numeric id of sample that you want to dump\n\
       (use data_stats program to see DSM ids and sample ids of data in a file)\n\
  -p: process (optional). Pass samples to sensor process method\n\
  -x xml_file (optional). Name of XML file (required with -p option)\n\
  -A: ASCII output (for samples from a serial sensor)\n\
  -F: floating point output (default for processed output)\n\
  -H: hex output (default for raw output)\n\
  -S: signed short output (useful for samples from an A2D)\n\
  URL (required). Either \"file:file_path\", \"sock:host:port\",\n\
      or simply a file_path.\n\
Examples:\n" <<
	argv0 << " -d 0 -s 100 /tmp/xxx.dat\n" <<
	argv0 << " -d 0 -s 100 file:/tmp/xxx.dat\n" <<
	argv0 << " -d 0 -s 200 -p -x ads3.xml sock:hyper:30000\n" << endl;
    exit(1);
}

DumpClient::DumpClient(dsm_sample_id_t id,format_t fmt,ostream &outstr):
	sampleId(id),format(fmt),ostr(outstr)
{
}

void DumpClient::printHeader()
{
    cout << "|--- date time -------|  bytes" << endl;
}

bool DumpClient::receive(const Sample* samp) throw()
{
    dsm_time_t tt = samp->getTimeTag();

    dsm_sample_id_t sampid = samp->getId();
    if (sampid != sampleId) return false;

    struct tm tm;
    char cstr[64];
    time_t ut = tt / USECS_PER_SEC;
    gmtime_r(&ut,&tm);
    int msec = (tt % USECS_PER_SEC) / USECS_PER_MSEC;
    strftime(cstr,sizeof(cstr),"%Y %m %d %H:%M:%S",&tm);
    ostr << cstr << '.' << setw(3) << setfill('0') << msec << ' ';
    ostr << setw(7) << setfill(' ') << samp->getDataByteLength() << ' ';

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
    case DEFAULT:
        break;
    }
    return true;
}

class FileDump
{
public:

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv);

    static bool interrupted;
};

bool FileDump::interrupted = false;

void FileDump::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            FileDump::interrupted = true;
    break;
    }
}

void FileDump::setupSignals()
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
    act.sa_sigaction = FileDump::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

int FileDump::main(int argc, char** argv)
{

    Runstring rstr(argc,argv);

    setupSignals();

    IOChannel* iochan = 0;

    if (rstr.dataFileName.length() > 0) {
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
	fset->setFileName(rstr.dataFileName);
#endif
    }
    else {
	atdUtil::Socket* sock = new atdUtil::Socket(rstr.hostName,rstr.port);
	iochan = new dsm::Socket(sock);
    }

    RawSampleInputStream sis(iochan);	// RawSampleStream now owns the iochan ptr.
    sis.init();

    auto_ptr<Project> project;
    list<DSMSensor*> allsensors;

    if (rstr.xmlFileName.length() > 0) {
	try {
	    auto_ptr<xercesc::DOMDocument> doc(
		DSMEngine::parseXMLConfigFile(rstr.xmlFileName));

	    project = auto_ptr<Project>(Project::getInstance());
	    project->fromDOMElement(doc->getDocumentElement());
	}
	catch (dsm::XMLException& e) {
	    cerr << e.what() << endl;
	    return 1;
	}
	catch (atdUtil::InvalidParameterException& e) {
	    cerr << e.what() << endl;
	    return 1;
	}

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

    DumpClient dumper(rstr.sampleId,rstr.format,cout);

    if (rstr.process) {
	list<DSMSensor*>::const_iterator si;
	for (si = allsensors.begin(); si != allsensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    sensor->init();
	    sis.addProcessedSampleClient(&dumper,sensor);
	}
    }
    else sis.addSampleClient(&dumper);

    dumper.printHeader();

    try {
	for (;;) {
	    sis.readSamples();
	    if (interrupted) break;
	}
    }
    catch (atdUtil::EOFException& eof) {
        cerr << eof.what() << endl;
    }
    catch (atdUtil::IOException& ioe) {
        cerr << ioe.what() << endl;
    }
    return 0;
}

int main(int argc, char** argv)
{
    return FileDump::main(argc,argv);
}
