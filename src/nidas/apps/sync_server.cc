// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

#include <nidas/dynld/raf/SyncRecordGenerator.h>
#include <nidas/core/FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/core/Project.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/util/Process.h>
#include <nidas/util/Logger.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

#include <unistd.h>
#include <getopt.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

namespace {
    int defaultLogLevel = n_u::LOGGER_INFO;
};

class SyncServer: public SampleConnectionRequester
{
public:

    SyncServer();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw(n_u::Exception);


// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void*);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    static const int DEFAULT_PORT = 30001;

    static const float SORTER_LENGTH_SECS = 2.0;

    void connect(SampleOutput* output) throw();

    void disconnect(SampleOutput* output) throw();

private:

    static bool _interrupted;

    string _xmlFileName;

    list<string> _dataFileNames;

    auto_ptr<n_u::SocketAddress> _servSocketAddr;

    float _sorterLengthSecs;

    static int _logLevel;
};

int main(int argc, char** argv)
{
    return SyncServer::main(argc,argv);
}


/* static */
bool SyncServer::_interrupted = false;

/* static */
int SyncServer::_logLevel = defaultLogLevel;

/* static */
void SyncServer::sigAction(int sig, siginfo_t* siginfo, void*) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            SyncServer::_interrupted = true;
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
    _xmlFileName(),_dataFileNames(),
    _servSocketAddr(new n_u::Inet4SocketAddress(DEFAULT_PORT)),
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
		if (ist.fail()) _servSocketAddr.reset(new n_u::UnixSocketAddress(optarg));
                else _servSocketAddr.reset(new n_u::Inet4SocketAddress(port));
	    }
	    break;
	case 'x':
	    _xmlFileName = optarg;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    for (; optind < argc; ) _dataFileNames.push_back(argv[optind++]);
    if (_dataFileNames.size() == 0) usage(argv[0]);
    return 0;
}

#ifdef PROJECT_IS_SINGLETON
class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};
#endif


void SyncServer::connect(SampleOutput* output) throw()
{
}

void SyncServer::disconnect(SampleOutput* output) throw()
{
    _interrupted = true;
}

int SyncServer::run() throw(n_u::Exception)
{

    try {

        Project project;

        IOChannel* iochan = 0;

        nidas::core::FileSet* fset =
            nidas::core::FileSet::getFileSet(_dataFileNames);

        iochan = fset->connect();

        // RawSampleStream owns the iochan ptr.
        RawSampleInputStream sis(iochan);

        // Apply some sample filters in case the file is corrupted.
        sis.setMaxDsmId(2000);
        sis.setMaxSampleLength(64000);
        sis.setMinSampleTime(n_u::UTime::parse(true,"2006 jan 1 00:00"));
        sis.setMaxSampleTime(n_u::UTime::parse(true,"2020 jan 1 00:00"));

	sis.readInputHeader();
	SampleInputHeader header = sis.getInputHeader();

	if (_xmlFileName.length() == 0)
	    _xmlFileName = header.getConfigName();
	_xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);

        {
            auto_ptr<xercesc::DOMDocument> doc(parseXMLConfigFile(_xmlFileName));
            project.fromDOMElement(doc->getDocumentElement());
        }

        XMLImplementation::terminate();

	set<DSMSensor*> sensors;
	SensorIterator ti = project.getSensorIterator();
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

	nidas::core::ServerSocket* servSock = new nidas::core::ServerSocket(*_servSocketAddr.get());

        // sync_server typically sits here, waiting for a connection before proceeding.
        IOChannel* ioc = servSock->connect();
        if (ioc != servSock) {
            servSock->close();
            delete servSock;
        }
        SampleOutputStream output(ioc,this);

        // don't try to reconnect. On an error in the output socket
        // writes will cease, but this process will keep reading samples.
        output.setReconnectDelaySecs(-1);
	syncGen.connect(&output);

        try {
            for (;;) {
                if (_interrupted) break;
                sis.readSamples();
            }
            sis.close();
            pipeline.flush();
            syncGen.disconnect(pipeline.getProcessedSampleSource());
            syncGen.disconnect(&output);
            output.close();
        }
        catch (n_u::EOFException& eof) {
            sis.close();
            pipeline.flush();
            syncGen.disconnect(pipeline.getProcessedSampleSource());
            syncGen.disconnect(&output);
            output.close();
            cerr << eof.what() << endl;
        }
        catch (n_u::IOException& ioe) {
            sis.close();
            pipeline.flush();
            syncGen.disconnect(pipeline.getProcessedSampleSource());
            syncGen.disconnect(&output);
            output.close();
            pipeline.interrupt();
            pipeline.join();
            throw(ioe);
        }
        pipeline.interrupt();
        pipeline.join();
    }
    catch (n_u::Exception& e) {
        cerr << e.what() << endl;
        XMLImplementation::terminate(); // ok to terminate() twice
	return 1;
    }
    SampleOutputRequestThread::destroyInstance();
    SamplePools::deleteInstance();
    return 0;
}
