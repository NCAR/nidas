/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-08-29 15:10:54 -0600 (Mon, 29 Aug 2005) $

    $LastChangedRevision: 2753 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.atd.ucar.edu/svn/hiaper/ads3/dsm/src/data_dump.cc $
 ********************************************************************

*/

#define _XOPEN_SOURCE	/* glibc2 needs this */
#include <time.h>

#include <FileSet.h>
#include <Socket.h>
#include <RawSampleInputStream.h>
#include <DSMEngine.h>
#include <SyncRecordGenerator.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace dsm;
using namespace std;

class Runstring {
public:
    Runstring(int argc, char** argv);

    static void usage(const char* argv0);

    static const int DEFAULT_PORT;
    string xmlFileName;
    string dataFileName;
    int port;
    bool simulationMode;
};

const int Runstring::DEFAULT_PORT = 30001;

Runstring::Runstring(int argc, char** argv):
	port(DEFAULT_PORT),simulationMode(false)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    xmlFileName = DSMServer::getADS3ConfigDir() + "/ads3.xml";
										
    while ((opt_char = getopt(argc, argv, "p:sx:")) != -1) {
	switch (opt_char) {
	case 'p':
	    {
		istringstream ist(optarg);
		ist >> port;
		if (ist.fail()) {
		    cerr << "Invalid port number: " << optarg << endl;
		    usage(argv[0]);
		}
	    }
	    break;
	case 's':
	    simulationMode = true;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }
    if (optind == argc - 1) dataFileName = argv[optind++];
    if (dataFileName.length() == 0) usage(argv[0]);
    if (optind != argc) usage(argv[0]);
}

void Runstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-x xml_file] [-p port] raw_data_file\n\
  -p port: sync record output socket port number: default=" << DEFAULT_PORT << "\n\
  -s: simulation mode (pause a second before sending each sync record)\n\
  -x xml_file (optional), default: $ADS3_CONFIG/projects/$ADS3_PROJECT/$ADS3_AIRCRAFT/flights/$ADS3_FLIGHT\n\
" << endl;
    exit(1);
}

class SyncServer
{
public:

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv);

    static bool interrupted;
};

bool SyncServer::interrupted = false;

void SyncServer::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            SyncServer::interrupted = true;
    break;
    }
}

void SyncServer::setupSignals()
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
    act.sa_sigaction = SyncServer::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

int SyncServer::main(int argc, char** argv)
{

    Runstring rstr(argc,argv);

    setupSignals();

    IOChannel* iochan = 0;

    try {
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

	RawSampleInputStream input(iochan);	// RawSampleStream owns the iochan ptr.
	input.init();

	auto_ptr<Project> project;

	auto_ptr<xercesc::DOMDocument> doc(
		DSMEngine::parseXMLConfigFile(rstr.xmlFileName));

	project = auto_ptr<Project>(Project::getInstance());
	project->fromDOMElement(doc->getDocumentElement());

	Site* site = 0;
	const list<Site*>& sites = project->getSites();

	const char* aircraft = getenv("ADS3_AIRCRAFT");

	list<Site*>::const_iterator ai = sites.begin();
	if (sites.size() == 1) site = *ai;
	else {
	    for (ai = sites.begin(); ai != sites.end(); ++ai) {
		Site* tmpsite = *ai;
		if (!tmpsite->getName().compare(aircraft)) {
		    site = tmpsite;
		    break;
		}
	    }
	}
	if (!site) throw atdUtil::InvalidParameterException("site",aircraft,
		"not found");

	const list<DSMConfig*>& dsms = site->getDSMConfigs();
	input.setDSMConfigs(dsms);

	list<DSMConfig*>::const_iterator di;
	for (di = dsms.begin(); di != dsms.end(); ++di) {
	    DSMConfig* dsm = *di;
	    dsm->initSensors();
	}

	SyncRecordGenerator syncGen;
	syncGen.connect(&input);

	dsm::ServerSocket* servSock = new dsm::ServerSocket(rstr.port);
	SampleOutputStream* output = new SampleOutputStream(servSock);

	output->connect();
	syncGen.connected(output);

	bool simulationMode = rstr.simulationMode;

	struct timespec nsleep;
	dsm_time_t nextDataSec = 0;
	dsm_time_t timeOffset = 0;
	dsm_time_t ttprev = 0;
	int granularity = USECS_PER_SEC / 10;
	for (;;) {
	    Sample* samp = input.readSample();
	    if (interrupted) break;

	    dsm_time_t tt = samp->getTimeTag();
	    if (simulationMode) {
		cerr << "tt=" << tt/USECS_PER_SEC << '.' <<
		    setfill('0') << setw(3) << (tt % USECS_PER_SEC) / 1000 <<
		    " nextDataSec=" <<  nextDataSec/USECS_PER_SEC << '.' <<
		    setfill('0') << setw(3) << ( nextDataSec % USECS_PER_SEC) / 1000 << endl;
		while (tt > nextDataSec) {
		    dsm_time_t tnow = getSystemTime();
		    long tsleep = granularity - (tnow % granularity);
		    // cerr << "tsleep=" << tsleep << endl;
		    nsleep.tv_sec = tsleep / USECS_PER_SEC;
		    nsleep.tv_nsec = (tsleep % USECS_PER_SEC) * 1000;
		    if (nanosleep(&nsleep,0) < 0 && errno == EINTR) break;

		    tnow += tsleep;

		    if (timeOffset == 0) {
		        timeOffset = tnow - tt;
			timeOffset -= (timeOffset % granularity);
		    }
		    // cerr << "tnow=" << tnow <<
		    // 	" timeOffset=" << timeOffset << endl;
		    nextDataSec = tnow - timeOffset + granularity;
		    // cerr << "nextDataSec=" << nextDataSec << endl;
		}
	    }
	    long ttdiff = tt - ttprev;
	    if (ttdiff < 0) cerr << "ttprev=" << ttprev << " tt=" << tt <<
	    	" ttdiff=" << ttdiff << endl;
	    ttprev = tt;
	    input.distribute(samp);
	    samp->freeReference();
	}
    }
    catch (atdUtil::EOFException& eof) {
        cerr << eof.what() << endl;
	return 1;
    }
    catch (atdUtil::IOException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (atdUtil::InvalidParameterException& ioe) {
        cerr << ioe.what() << endl;
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
    return SyncServer::main(argc,argv);
}
