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
#include <nidas/util/Process.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

namespace {
    int defaultLogLevel = n_u::LOGGER_INFO;
};

class SyncServer
{
public:

    SyncServer();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw(n_u::Exception);


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

    float _sorterLengthSecs;

    static int _logLevel;
};

int main(int argc, char** argv)
{
    return SyncServer::main(argc,argv);
}


/* static */
bool SyncServer::interrupted = false;

/* static */
int SyncServer::_logLevel = defaultLogLevel;

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
    -l sorterSecs: length of sample sorter, in fractional seconds\n\
        default=" << (float)SORTER_LENGTH_SECS << "\n\
    -L loglevel: set logging level, 7=debug,6=info,5=notice,4=warning,3=err,...\n\
        The default level is " << defaultLogLevel << "\n\
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

    logger = n_u::Logger::createInstance(&std::cerr);
    lc.level = _logLevel;

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
    _sorterLengthSecs(SORTER_LENGTH_SECS)
{
}

int SyncServer::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "L:l:p:x:")) != -1) {
	switch (opt_char) {
        case 'l':
            {
		istringstream ist(optarg);
		ist >> _sorterLengthSecs;
		if (ist.fail()) return usage(argv[0]);
            }
            break;
        case 'L':
            {
		istringstream ist(optarg);
		ist >> _logLevel;
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
	    return usage(argv[0]);
	}
    }
    for (; optind < argc; ) dataFileNames.push_back(argv[optind++]);
    if (dataFileNames.size() == 0) usage(argv[0]);
    return 0;
}

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

int SyncServer::run() throw(n_u::Exception)
{

    try {

        AutoProject aproject;

        IOChannel* iochan = 0;

        nidas::core::FileSet* fset =
            nidas::core::FileSet::getFileSet(dataFileNames);

        iochan = fset->connect();

        // RawSampleStream owns the iochan ptr.
        RawSampleInputStream sis(iochan);

	sis.readInputHeader();
	SampleInputHeader header = sis.getInputHeader();

	if (xmlFileName.length() == 0)
	    xmlFileName = header.getConfigName();
	xmlFileName = n_u::Process::expandEnvVars(xmlFileName);

        {
            auto_ptr<xercesc::DOMDocument> doc(
                    DSMEngine::parseXMLConfigFile(xmlFileName));
            Project::getInstance()->fromDOMElement(doc->getDocumentElement());
        }

	set<DSMSensor*> sensors;
	SensorIterator ti = Project::getInstance()->getSensorIterator();
	for ( ; ti.hasNext(); ) {
	    DSMSensor* sensor = ti.next();
            if (sensors.insert(sensor).second) {
                sis.addSampleTag(sensor->getRawSampleTag());
	        sensors.insert(sensor);
		sensor->init();
	    }
	}

        SamplePipeline pipeline;
        pipeline.setRealTime(false);
        pipeline.setRawSorterLength(1.0);
        pipeline.setProcSorterLength(_sorterLengthSecs);
	
	// Even though the time length of the raw sorter is typically
	// much smaller than the length of the processed sample sorter,
	// (currently 1 second vs 900 seconds) its heap size needs to be
	// proportionally larger since the raw samples include the fast
	// 2DC data, and the processed 2DC samples are much smaller.
	// Note that if more memory than this is needed to sort samples
	// over the length of the sorter, then the heap is dynamically
	// increased. There isn't much penalty in choosing too small of
	// a value.
	pipeline.setRawHeapMax(50 * 1000 * 1000);
	pipeline.setProcHeapMax(100 * 1000 * 1000);
        pipeline.connect(&sis);

        SyncRecordGenerator syncGen;
	syncGen.connect(pipeline.getProcessedSampleSource());

	nidas::core::ServerSocket* servSock = new nidas::core::ServerSocket(*addr.get());
        IOChannel* ioc = servSock->connect();
        if (ioc != servSock) {
            servSock->close();
            delete servSock;
        }
        SampleOutputStream output(ioc);
	syncGen.connect(&output);

        try {
            for (;;) {
                if (interrupted) break;
                sis.readSamples();
            }
            sis.flush();
            sis.close();
            syncGen.disconnect(pipeline.getProcessedSampleSource());
            syncGen.disconnect(&output);
            output.close();
        }
        catch (n_u::EOFException& eof) {
            sis.flush();
            sis.close();
            syncGen.disconnect(pipeline.getProcessedSampleSource());
            syncGen.disconnect(&output);
            output.close();
            cerr << eof.what() << endl;
        }
        catch (n_u::IOException& ioe) {
            sis.flush();
            sis.close();
            syncGen.disconnect(pipeline.getProcessedSampleSource());
            syncGen.disconnect(&output);
            output.close();
            throw(ioe);
        }
    }
    catch (n_u::Exception& e) {
        cerr << e.what() << endl;
	return 1;
    }
    return 0;
}
