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
#include <ctime>

#include <nidas/core/FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/dynld/raf/SyncRecordGenerator.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

class SyncServer
{
public:

    SyncServer();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

    void simLoop(SampleInputStream& input,SampleOutputStream* output,
	SyncRecordGenerator& syncGen) throw(n_u::IOException);

    void normLoop(SampleInputStream& input,SampleOutputStream* output,
	SyncRecordGenerator& syncGen) throw(n_u::IOException);

    bool debug() const { return _debug; }


// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    static const int DEFAULT_PORT = 30001;

    static const int SORTER_LENGTH_MSECS = 2000;

private:

    static bool interrupted;

    string xmlFileName;

    list<string> dataFileNames;

    auto_ptr<n_u::SocketAddress> addr;

    bool simulationMode;

    bool _debug;

    int _sorterLengthMsecs;
};

int main(int argc, char** argv)
{
    return SyncServer::main(argc,argv);
}


/* static */
bool SyncServer::interrupted = false;

/* static */
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

/* static */
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

/* static */
int SyncServer::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-l sorterSecs] [-x xml_file] [-p port] raw_data_file ...\n\
    -d: debug. Log messages to stderr instead of syslog\n\
    -l sorterSecs: length of sample sorter, in fractional seconds\n\
        default=" << (float)SORTER_LENGTH_MSECS / MSECS_PER_SEC << "\n\
    -p port: sync record output socket port number: default=" << DEFAULT_PORT << "\n\
    -s: simulation mode (pause a second before sending each sync record)\n\
    -x xml_file (optional), default: \n\
	$ADS3_CONFIG/projects/<project>/<aircraft>/flights/<flight>/ads3.xml\n\
	where <project>, <aircraft> and <flight> are read from the input data header\n\
    raw_data_file: names of one or more raw data files, separated by spaces\n\
" << endl;
    return 1;
}

/* static */
int SyncServer::main(int argc, char** argv) throw()
{
    setupSignals();

    SyncServer sync;

    int res;
    n_u::LogConfig lc;
    n_u::Logger* logger;
    
    if ((res = sync.parseRunstring(argc,argv)) != 0) return res;

    // If user passed -d option, send all log messages to cerr,
    // otherwise to syslog.
    if (!sync.debug()) {
	logger = n_u::Logger::createInstance("dsm",LOG_CONS,LOG_LOCAL5);
        // Configure default logging to log anything NOTICE and above.
        lc.level = n_u::LOGGER_INFO;
    }
    else
    {
	logger = n_u::Logger::createInstance(&std::cerr);
        lc.level = n_u::LOGGER_DEBUG;
    }

    logger->setScheme(n_u::LogScheme().addConfig (lc));
    return sync.run();
}


SyncServer::SyncServer():
    addr(new n_u::Inet4SocketAddress(DEFAULT_PORT)),
    simulationMode(false),_debug(false),
    _sorterLengthMsecs(SORTER_LENGTH_MSECS)
{
}

int SyncServer::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "dl:p:sx:")) != -1) {
	switch (opt_char) {
	case 'd':
            _debug = true;
            break;
        case 'l':
            {
                float secs;
		istringstream ist(optarg);
		ist >> secs;
		if (ist.fail()) return usage(argv[0]);
                _sorterLengthMsecs = (int)rint(secs * MSECS_PER_SEC);
            }
            break;
	case 'p':
	    {
                int port;
		istringstream ist(optarg);
		ist >> port;
		if (ist.fail()) addr.reset(new n_u::UnixSocketAddress(optarg));
                else addr.reset(new n_u::Inet4SocketAddress(port));
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
    for (; optind < argc; ) dataFileNames.push_back(argv[optind++]);
    if (dataFileNames.size() == 0) usage(argv[0]);
    return 0;
}

int SyncServer::run() throw()
{

    IOChannel* iochan = 0;

    try {
	nidas::core::FileSet* fset = new nidas::core::FileSet();
	iochan = fset;

	list<string>::const_iterator fi;
	for (fi = dataFileNames.begin(); fi != dataFileNames.end(); ++fi)
	    fset->addFileName(*fi);

	// SortedSampleStream owns the iochan ptr.
	SortedSampleInputStream input(iochan);
	input.setSorterLengthMsecs(_sorterLengthMsecs);

	// Block while waiting for heapSize to become less than heapMax.
	input.setHeapBlock(true);
	input.setHeapMax(500000000);
	input.init();

	input.readInputHeader();
	SampleInputHeader header = input.getInputHeader();

	if (xmlFileName.length() == 0)
	    xmlFileName = header.getConfigName();
	xmlFileName = Project::expandEnvVars(xmlFileName);

	auto_ptr<xercesc::DOMDocument> doc(
		DSMEngine::parseXMLConfigFile(xmlFileName));

	auto_ptr<Project> project(Project::getInstance());

	project->fromDOMElement(doc->getDocumentElement());

	set<DSMSensor*> sensors;
	SensorIterator ti = project->getSensorIterator();
	for ( ; ti.hasNext(); ) {
	    DSMSensor* sensor = ti.next();
	    input.addSampleTag(sensor->getRawSampleTag());

	    set<DSMSensor*>::const_iterator si = sensors.find(sensor);
	    if (si == sensors.end()) {
	        sensors.insert(sensor);
		sensor->init();
	    }
	}

	SyncRecordGenerator syncGen;
	syncGen.connect(&input);

	nidas::core::ServerSocket* servSock = new nidas::core::ServerSocket(*addr.get());
        // For post processing, write as fast as you can
        servSock->setMinWriteInterval(0);
        servSock->setNonBlocking(false);
	SampleOutputStream* output = new SampleOutputStream(servSock);

	output->connect();
	syncGen.connect(output,output);

	if (simulationMode) simLoop(input,output,syncGen);
	else normLoop(input,output,syncGen);
    }
    catch (n_u::EOFException& eof) {
        cerr << eof.what() << endl;
	return 1;
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (n_u::InvalidParameterException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (XMLException& e) {
        cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::Exception& e) {
        cerr << e.what() << endl;
	return 1;
    }
    return 0;
}

void SyncServer::simLoop(SampleInputStream& input,SampleOutputStream* output,
	SyncRecordGenerator& syncGen) throw(n_u::IOException)
{

    try {
	Sample* samp = input.readSample();
	dsm_time_t tt = samp->getTimeTag();
	input.distribute(samp);

	int simClockRes = USECS_PER_SEC / 10;	// simulated clock resolution

	// simulated data clock. Round it up to next simClockRes.
	dsm_time_t simClock = tt + simClockRes - (tt % simClockRes);

	const int MAX_WAIT = 5;

	for (;;) {
	    if (!output->getIOStream()) break;	 // check for disconnect
	    if (interrupted) break;

	    samp = input.readSample();

	    tt = samp->getTimeTag();

	    while (tt > simClock) {	// getting ahead of simulated data clock

#ifdef DEBUG
	        cerr << "tt=" << tt / USECS_PER_SEC << '.' <<
			setfill('0') << setw(3) << (tt % USECS_PER_SEC) / 1000 <<
		    " simClock=" <<  simClock / USECS_PER_SEC << '.' <<
		    	setfill('0') << setw(3) <<  (simClock % USECS_PER_SEC) / 1000 <<
		    endl;
#endif
			
		// correct for drift
		long tsleep = simClockRes - (getSystemTime() % simClockRes);
		struct timespec nsleep;
		nsleep.tv_sec = tsleep / USECS_PER_SEC;
		nsleep.tv_nsec = (tsleep % USECS_PER_SEC) * 1000;
		if (nanosleep(&nsleep,0) < 0 && errno == EINTR) break;

		simClock += simClockRes;

		int tdiff = (int)((tt - simClock) / USECS_PER_SEC);
		// if a big jump in the data, wait a max of 5 seconds for the impatient.
		if (tdiff > MAX_WAIT) 
		    simClock += (tdiff - MAX_WAIT) * USECS_PER_SEC;
	    }
	    input.distribute(samp);
	}
    }
    catch (n_u::EOFException& e) {
	input.close();
	syncGen.disconnect(&input);
	syncGen.disconnect(output);
	throw e;
    }
    catch (n_u::IOException& e) {
	input.close();
	syncGen.disconnect(&input);
	syncGen.disconnect(output);
	throw e;
    }
}

void SyncServer::normLoop(SampleInputStream& input,SampleOutputStream* output,
	SyncRecordGenerator& syncGen) throw(n_u::IOException)
{

    try {
	for (;;) {
	    if (interrupted) break;
	    input.readSamples();
	}
    }
    catch (n_u::EOFException& e) {
	cerr << "EOF received: flushing buffers" << endl;
	input.flush();
	syncGen.disconnect(&input);

	input.close();
	syncGen.disconnect(output);
	throw e;
    }
    catch (n_u::IOException& e) {
	input.close();
	syncGen.disconnect(&input);
	syncGen.disconnect(output);
	throw e;
    }
}
