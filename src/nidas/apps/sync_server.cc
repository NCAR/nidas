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
#include <nidas/dynld/RawSampleInputStream.h>
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

    int run() throw(n_u::Exception);

    bool debug() const { return _debug; }


// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    static const int DEFAULT_PORT = 30001;

    static const float SORTER_LENGTH_SECS = 2.0;

private:

    static bool interrupted;

    string xmlFileName;

    list<string> dataFileNames;

    auto_ptr<n_u::SocketAddress> addr;

    bool _debug;

    float _sorterLengthSecs;
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
        default=" << (float)SORTER_LENGTH_SECS << "\n\
    -p port: sync record output socket port number: default=" << DEFAULT_PORT << "\n\
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

    try {
        return sync.run();
    }
    catch(const n_u::Exception&e ) {
        cerr << e.what() << endl;
        return 1;
    }
}

SyncServer::SyncServer():
    addr(new n_u::Inet4SocketAddress(DEFAULT_PORT)),
    _debug(false),
    _sorterLengthSecs(SORTER_LENGTH_SECS)
{
}

int SyncServer::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "dl:p:x:")) != -1) {
	switch (opt_char) {
	case 'd':
            _debug = true;
            break;
        case 'l':
            {
		istringstream ist(optarg);
		ist >> _sorterLengthSecs;
		if (ist.fail()) return usage(argv[0]);
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

int SyncServer::run() throw(n_u::Exception)
{

    IOChannel* iochan = 0;

    SamplePipeline pipeline;
    SyncRecordGenerator syncGen;

    nidas::core::FileSet* fset = new nidas::core::FileSet();
    iochan = fset;

    list<string>::const_iterator fi;
    for (fi = dataFileNames.begin(); fi != dataFileNames.end(); ++fi)
        fset->addFileName(*fi);

    // RawSampleStream owns the iochan ptr.
    RawSampleInputStream sis(iochan);

    SampleOutputStream* output = 0;

    try {
	sis.readInputHeader();
	SampleInputHeader header = sis.getInputHeader();

        pipeline.setRealTime(false);
        pipeline.setRawSorterLength(1.0);
        pipeline.setProcSorterLength(_sorterLengthSecs);
	pipeline.setRawHeapMax(100* 1000 * 1000);
	pipeline.setProcHeapMax(1000* 1000 * 1000);
        pipeline.connect(&sis);

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
	    sis.addSampleTag(sensor->getRawSampleTag());

	    set<DSMSensor*>::const_iterator si = sensors.find(sensor);
	    if (si == sensors.end()) {
	        sensors.insert(sensor);
		sensor->init();
	    }
	}

	syncGen.connect(pipeline.getProcessedSampleSource());

	nidas::core::ServerSocket* servSock = new nidas::core::ServerSocket(*addr.get());
        // For post processing, write as fast as you can
        servSock->setMinWriteInterval(0);
        servSock->setNonBlocking(false);
	output = new SampleOutputStream(servSock);
	syncGen.connect(output);

        for (;;) {
            if (interrupted) break;
            sis.readSamples();
        }
    }
    catch (n_u::EOFException& eof) {
        sis.flush();
        sis.close();
	syncGen.disconnect(pipeline.getProcessedSampleSource());
	if (output) {
            syncGen.disconnect(output);
            output->close();
        }
        cerr << eof.what() << endl;
	return 0;
    }
    catch (n_u::IOException& ioe) {
        sis.flush();
        sis.close();
	syncGen.disconnect(pipeline.getProcessedSampleSource());
	syncGen.disconnect(output);
        sis.close();
	if (output) {
            syncGen.disconnect(output);
            output->close();
        }
        throw(ioe);
    }
    return 0;
}
