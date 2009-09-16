/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/DSMEngine.h>

#include <nidas/core/XMLStringConverter.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/Version.h>

#include <nidas/core/XMLConfigInput.h>
#include <nidas/core/XMLFdInputSource.h>

#include <nidas/core/SampleIOProcessor.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/util/Process.h>

#include <iostream>
#include <fstream>
#include <limits>
#include <memory>  // auto_ptr<>
#include <pwd.h>
#include <sys/resource.h>
#include <sys/mman.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

namespace {
    int defaultLogLevel = n_u::LOGGER_NOTICE;
};

/* static */
DSMEngine* DSMEngine::_instance = 0;

DSMEngine::DSMEngine():
    _externalControl(false),_runState(RUNNING),_nextState(RUN),
    _syslogit(true),
    _project(0),
    _dsmConfig(0),_selector(0),_pipeline(0),
    _statusThread(0),_xmlrpcThread(0),
    _clock(SampleClock::getInstance()),
    _xmlRequestSocket(0),_rtlinux(-1),_userid(0),_groupid(0),
    _logLevel(defaultLogLevel)
{
    try {
	_configSockAddr = n_u::Inet4SocketAddress(
	    n_u::Inet4Address::getByName(NIDAS_MULTICAST_ADDR),
	    NIDAS_SVC_REQUEST_PORT_UDP);
    }
    catch(const n_u::UnknownHostException& e) {	// shouldn't happen
        cerr << e.what();
   }
   SampleOutputRequestThread::getInstance()->start();
}

DSMEngine::~DSMEngine()
{
    delete _statusThread;
    delete _xmlrpcThread;
    delete _xmlRequestSocket;
   SampleOutputRequestThread::destroyInstance();
    delete _project;
}

namespace {
    void getPageFaults(long& minor,long& major, long& nswap) 
    {
        struct rusage r;
        getrusage(RUSAGE_SELF,&r);
        minor = r.ru_minflt;
        major = r.ru_majflt;
        nswap = r.ru_nswap;
    }
    void logPageFaultDiffs(long minor,long major, long nswap)
    {
        long minflts,majflts,nswap2;
        getPageFaults(minflts,majflts,nswap2);

        n_u::Logger::getInstance()->log(LOG_INFO,"page faults: minor=%d, major=%d, swaps=%d",
            minflts-minor,majflts-major,nswap2-nswap);
    }
}

/* static */
int DSMEngine::main(int argc, char** argv) throw()
{
    DSMEngine engine;

    int res;
    if ((res = engine.parseRunstring(argc,argv)) != 0) return res;

    engine.initLogger();

#ifdef CAP_SYS_NICE
    try {
        n_u::Process::addEffectiveCapability(CAP_SYS_NICE);
        n_u::Process::addEffectiveCapability(CAP_IPC_LOCK);
#ifdef DEBUG
        DLOG(("CAP_SYS_NICE = ") << n_u::Process::getEffectiveCapability(CAP_SYS_NICE));
        DLOG(("PR_GET_SECUREBITS=") << hex << prctl(PR_GET_SECUREBITS,0,0,0,0) << dec);
#endif
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s: %s. Will not be able to use real-time priority",argv[0],e.what()));
    }
#endif

    gid_t gid = engine.getGroupID();
    if (gid != 0 && getegid() != gid) {
        DLOG(("doing setgid(%d)",gid));
        if (setgid(gid) < 0)
            WLOG(("%s: cannot change group id to %d: %m","dsm",gid));
    }

    uid_t uid = engine.getUserID();
    if (uid != 0 && geteuid() != uid) {
        DLOG(("doing setuid(%d=%s)",uid,engine.getUserName().c_str()));
        if (setuid(uid) < 0)
            WLOG(("%s: cannot change userid to %d (%s): %m", "dsm",
                uid,engine.getUserName().c_str()));
    }

    // Open and check the pid file after the above daemon() call.
    try {
        pid_t pid = n_u::Process::checkPidFile("/tmp/dsm.pid");
        if (pid > 0) {
            CLOG(("dsm process, pid=%d is already running",pid));
            return 1;
        }
    }
    catch(const n_u::IOException& e) {
        CLOG(("dsm: %s",e.what()));
        return 1;
    }

#ifdef DO_MLOCKALL
    ILOG(("Locking memory: mlockall(MCL_CURRENT | MCL_FUTURE)"));
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        n_u::IOException e("dsm","mlockall",errno);
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s",e.what());
    }
#endif
    long minflts,majflts,nswap;
    getPageFaults(minflts,majflts,nswap);

    // Set the singleton instance
    _instance = &engine;

    setupSignals();

    try {
        engine.startXmlRpcThread();

        res = engine.run();		// doesn't throw exceptions

        engine.killXmlRpcThread();
    }
    catch (const n_u::Exception &e) {
        PLOG(("%s",e.what()));
    }

    unsetupSignals();

    // All users of singleton instance should have been shut down.
    _instance = 0;

    logPageFaultDiffs(minflts,majflts,nswap);

    return res;
}

int DSMEngine::parseRunstring(int argc, char** argv) throw()
{
    // extern char *optarg;  /* set by getopt()  */
    extern int optind;       /*  "  "     "      */
    int opt_char;            /* option character */

    while ((opt_char = getopt(argc, argv, "dl:ru:v")) != -1) {
	switch (opt_char) {
	case 'd':
	    _syslogit = false;
            _logLevel = n_u::LOGGER_DEBUG;
	    break;
	case 'l':
            _logLevel = atoi(optarg);
	    break;
	case 'r':
	    _externalControl = true;
	    break;
	case 'u':
            {
                struct passwd pwdbuf;
                struct passwd *result;
                long nb = sysconf(_SC_GETPW_R_SIZE_MAX);
                if (nb < 0) nb = 4096;
                vector<char> strbuf(nb);
                if (getpwnam_r(optarg,&pwdbuf,&strbuf.front(),nb,&result) < 0) {
                    cerr << "Unknown user: " << optarg << endl;
                    usage(argv[0]);
                    return 1;
                }
                _username = optarg;
                _userid = pwdbuf.pw_uid;
                _groupid = pwdbuf.pw_gid;
            }
	    break;
	case 'v':
	    cout << Version::getSoftwareVersion() << endl;
	    exit(1);
	    break;
	case '?':
	    usage(argv[0]);
	    return 1;
	}
    }
    if (optind == argc - 1) {
        string url = string(argv[optind++]);
        string type = "file";
        string::size_type ic = url.find(':');
        if (ic != string::npos) type = url.substr(0,ic);
        if (type == "sock" || type == "inet" || type == "mcsock") {
	    url = url.substr(ic+1);
	    ic = url.find(':');
	    string addr = url.substr(0,ic);
            if (addr.length() == 0 && type == "mcsock") addr = NIDAS_MULTICAST_ADDR;
	    int port = NIDAS_SVC_REQUEST_PORT_UDP;
	    if (ic != string::npos) {
		istringstream ist(url.substr(ic+1));
		ist >> port;
		if (ist.fail()) {
		    cerr << "Invalid port number: " << url.substr(ic+1) << endl;
		    usage(argv[0]);
		    return 1;
		}
	    }
	    try {
		_configSockAddr = n_u::Inet4SocketAddress(
		    n_u::Inet4Address::getByName(addr),port);
                // cerr << "sock addr=" << _configSockAddr.toString() << endl;
	    }
	    catch(const n_u::UnknownHostException& e) {
	        cerr << e.what() << endl;
		usage(argv[0]);
		return 1;
	    }	
	}
        else if (type == "file") _configFile = url;
        else {
            cerr << "unknown url: " << url << endl;
            usage(argv[0]);
            return 1;
        }
    }

    if (optind != argc) {
	usage(argv[0]);
	return 1;
    }
    return 0;
}

void DSMEngine::usage(const char* argv0) 
{
    cerr << "\
Usage: " << argv0 << " [-d ] [-l loglevel] [-v] [ config ]\n\n\
  -d: debug, run in foreground and send messages to stderr with log level of debug\n\
      Otherwise run in the background, cd to /, and log messages to syslog\n\
      Specify a -l option after -d to change the log level from debug\n\
  -l loglevel: set logging level, 7=debug,6=info,5=notice,4=warning,3=err,...\n\
     The default level if no -d option is " << defaultLogLevel << "\n\
  -r: rpc, start XML RPC thread to respond to external commands\n\
  -v: display software version number and exit\n\
  config: either the name of a local DSM configuration XML file to be read,\n\
      or a socket address in the form \"sock:addr:port\".\n\
The default config is \"sock:" <<
	NIDAS_MULTICAST_ADDR << ":" << NIDAS_SVC_REQUEST_PORT_UDP << "\"" << endl;
}

void DSMEngine::initLogger()
{
    nidas::util::Logger* logger = 0;
    n_u::LogConfig lc;
    lc.level = _logLevel;
    if (_syslogit) {
	// fork to background
	if (daemon(0,0) < 0) {
	    n_u::IOException e("DSMEngine","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
	logger = n_u::Logger::createInstance("dsm",LOG_CONS,LOG_LOCAL5);
    }
    else
    {
	logger = n_u::Logger::createInstance(&std::cerr);
    }
    logger->setScheme(n_u::LogScheme("dsm").addConfig (lc));
}

int DSMEngine::run() throw()
{
    xercesc::DOMDocument* projectDoc = 0;

    for (; _nextState != QUIT; ) {

        // cleanup before re-starting the loop
        joinDataThreads();

        disconnectProcessors();

        closeOutputs();

        if (projectDoc) {
            projectDoc->release();
            projectDoc = 0;
        }

        if (_project) {
            delete _project;
            _project = 0;
            _dsmConfig = 0;
        }

        // One of the data threads is the SensorHandler. Deleting the SensorHandler
        // deletes all the sensors. They should be deleted after the
        // project is deleted since various objects in the project may
        // hold references to the sensors.
        deleteDataThreads();

        if (_runState == ERROR && _nextState != STOP) sleep(15);

        if (_nextState == STOP) {
            // wait on the _runCond condition variable
            _runCond.lock();
            while (_nextState == STOP) _runCond.wait();
            _runCond.unlock();
        }
        if (_nextState == RESTART) _nextState = RUN;
        if (_nextState != RUN) continue;

        // first fetch the configuration
        try {
            if (_configFile.length() == 0)
                projectDoc = requestXMLConfig(_configSockAddr);
            else {
                // expand environment variables in name
                string expName = n_u::Process::expandEnvVars(_configFile);
                projectDoc = parseXMLConfigFile(expName);
            }
        }
        catch (const XMLException& e) {
            CLOG(("%s",e.what()));
            _runState = ERROR;
            continue;
        }
        catch (const n_u::Exception& e) {
            // DSMEngine::interrupt() does an _xmlRequestSocket->close(),
            // which will throw an IOException in requestXMLConfig 
            // if we were still waiting for the XML config.
            CLOG(("%s",e.what()));
            _runState = ERROR;
            continue;
        }

        if (_nextState != RUN) continue;

        // then initialize the DSMEngine
        try {
            initialize(projectDoc);
        }
        catch (const n_u::InvalidParameterException& e) {
            CLOG(("%s",e.what()));
            _runState = ERROR;
            continue;
        }
        projectDoc->release();
        projectDoc = 0;

        if (_dsmConfig->getDerivedDataSocketAddr().getPort() != 0) {
	    try {
              DerivedDataReader::createInstance(_dsmConfig->getDerivedDataSocketAddr());
	    }
	    catch(n_u::IOException&e) {
                PLOG(("%s",e.what()));
	    }
	}
        // start your sensors
        try {
            openSensors();
            connectOutputs();
            connectProcessors();
        }
        catch (const n_u::IOException& e) {
            CLOG(("%s",e.what()));
            _runState = ERROR;
            continue;
        }
        catch (const n_u::InvalidParameterException& e) {
            CLOG(("%s",e.what()));
            _runState = ERROR;
            continue;
        }
        if (_nextState != RUN) continue;

        // start the status Thread
        if (_dsmConfig->getStatusSocketAddr().getPort() != 0) {
            _statusThread = new DSMEngineStat("DSMEngineStat",_dsmConfig->getStatusSocketAddr());
            _statusThread->start();
	}
        _runState = RUNNING;

        _runCond.lock();
        while (_nextState == RUN) _runCond.wait();
        _runCond.unlock();

        if (_nextState == QUIT) break;
        interrupt();
    }   // Run loop

    interrupt();

    joinDataThreads();

    disconnectProcessors();

    closeOutputs();

    if (projectDoc) {
        projectDoc->release();
        projectDoc = 0;
    }

    if (_project) {
        delete _project;
        _project = 0;
        _dsmConfig = 0;
    }

    deleteDataThreads();

    return _runState == ERROR;
}

void DSMEngine::interrupt()
{
    // If DSMEngine is waiting for an XML connection, closing the
    // _xmlRequestSocket here will cause an IOException in
    // DSMEngine::requestXMLConfig().
    _xmlRequestMutex.lock();
    if (_xmlRequestSocket) _xmlRequestSocket->close();
    _xmlRequestMutex.unlock();

    if (_statusThread) _statusThread->interrupt();
    if (DerivedDataReader::getInstance()) DerivedDataReader::getInstance()->interrupt();
    if (_selector) _selector->interrupt();
}

void DSMEngine::joinDataThreads() throw()
{
    // stop/join the status thread before closing sensors.
    // The status thread also loops over sensors.
    if (_statusThread) {
        try {
            _statusThread->join();
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
        }
    }

    if (_pipeline) _pipeline->flush();

    if (_selector) {
        try {
            _selector->join();
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
        }
    }

    if (DerivedDataReader::getInstance()) {
        try {
            if (DerivedDataReader::getInstance()->isRunning()) {
                DerivedDataReader::getInstance()->interrupt();
                DerivedDataReader::getInstance()->cancel();
            }
            DerivedDataReader::getInstance()->join();
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
        }
    }
}

void DSMEngine::deleteDataThreads() throw()
{
    if (_statusThread) {
        delete _statusThread;
        _statusThread = 0;
    }

    if (_selector) {
        delete _selector;	// this closes any still-open sensors
        _selector = 0;
    }

    if (DerivedDataReader::getInstance()) {
        DerivedDataReader::deleteInstance();
    }

    delete _pipeline;
    _pipeline = 0;
}

void DSMEngine::start()
{
    _runCond.lock();
    _nextState = RUN;
    _runCond.signal();
    _runCond.unlock();
}

/*
 * Stop data acquisition threads and wait for next command.
 */
void DSMEngine::stop()
{
    _runCond.lock();
    _nextState = STOP;
    _runCond.signal();
    _runCond.unlock();
}

void DSMEngine::restart()
{
    _runCond.lock();
    _nextState = RESTART;
    _runCond.signal();
    _runCond.unlock();
}

void DSMEngine::quit()
{
    _runCond.lock();
    _nextState = QUIT;
    _runCond.signal();
    _runCond.unlock();
}

/* static */
void DSMEngine::setupSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGINT);
    sigaddset(&sigset,SIGUSR1);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);

    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = DSMEngine::sigAction;
    sigaction(SIGUSR1,&act,(struct sigaction *)0);
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

void DSMEngine::unsetupSignals()
{
    sigset_t sigset;

    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_handler = SIG_IGN;
    sigaction(SIGUSR1,&act,(struct sigaction *)0);
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
void DSMEngine::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    n_u::Logger::getInstance()->log(LOG_INFO,
    	"received signal %s(%d), si_signo=%d, si_errno=%d, si_code=%d",
	strsignal(sig),sig,
	(siginfo ? siginfo->si_signo : -1),
	(siginfo ? siginfo->si_errno : -1),
	(siginfo ? siginfo->si_code : -1));
    switch(sig) {
    case SIGHUP:
      DSMEngine::getInstance()->restart();
      break;
    case SIGTERM:
    case SIGINT:
    case SIGUSR1:
      DSMEngine::getInstance()->quit();
      break;
    }
}

void DSMEngine::startXmlRpcThread() throw(n_u::Exception)
{
    // start the xmlrpc control thread
    if (_externalControl) {
        _xmlrpcThread = new DSMEngineIntf();
        _xmlrpcThread->start();
    }
}

void DSMEngine::killXmlRpcThread() throw()
{
    if (_xmlrpcThread) {
        try {
            if (_xmlrpcThread->isRunning()) {
                n_u::Logger::getInstance()->log(LOG_INFO,
                    "DSMEngine::interrupt, cancelling xmlrpcThread");
                // if this is running under valgrind, then cancel doesn't
                // work, but a kill(SIGUSR1) does.  Otherwise a cancel works.
                // _xmlrpcThread->cancel();
                _xmlrpcThread->kill(SIGUSR1);
            }
            n_u::Logger::getInstance()->log(LOG_INFO,
                "DSMEngine::interrupt, joining xmlrpcThread");
           _xmlrpcThread->join();
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
        }
       delete _xmlrpcThread;
       _xmlrpcThread = 0;
    }

}

xercesc::DOMDocument* DSMEngine::requestXMLConfig(
	const n_u::Inet4SocketAddress &mcastAddr)
	throw(n_u::Exception)
{

    auto_ptr<XMLParser> parser(new XMLParser());
    // throws XMLException

    // If parsing xml received from a server over a socket,
    // turn off validation - assume the server has validated the XML.
    parser->setDOMValidation(false);
    parser->setDOMValidateIfSchema(false);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(false);
    parser->setXercesSchemaFullChecking(false);
    parser->setDOMDatatypeNormalization(false);
    parser->setXercesUserAdoptsDOMDocument(true);

    _xmlRequestMutex.lock();
    delete _xmlRequestSocket;
    _xmlRequestSocket = new XMLConfigInput();
    _xmlRequestSocket->setInet4McastSocketAddress(mcastAddr);
    _xmlRequestMutex.unlock();

    auto_ptr<n_u::Socket> configSock;
    try {
        n_u::Inet4PacketInfoX pktinfo;
        configSock.reset(_xmlRequestSocket->connect(pktinfo));
    }
    catch(...) {
        _xmlRequestMutex.lock();
        if (_xmlRequestSocket) {
            _xmlRequestSocket->close();
            delete _xmlRequestSocket;
            _xmlRequestSocket = 0;
        }
        _xmlRequestMutex.unlock();
        throw;
    }

    _xmlRequestMutex.lock();
    if (_xmlRequestSocket) {
        _xmlRequestSocket->close();
        delete _xmlRequestSocket;
        _xmlRequestSocket = 0;
    }
    _xmlRequestMutex.unlock();

    xercesc::DOMDocument* doc = 0;
    try {
        std::string sockName = configSock->getRemoteSocketAddress().toString();
        XMLFdInputSource sockSource(sockName,configSock->getFd());
	doc = parser->parse(sockSource);
        configSock->close();
    }
    catch(const n_u::IOException& e) {
        PLOG(("DSMEngine::requestXMLConfig:") << e.what());
        configSock->close();
        throw e;
    }
    catch(const nidas::core::XMLException& xe) {
        PLOG(("DSMEngine::requestXMLConfig:") << xe.what());
        configSock->close();
        throw xe;
    }
    catch(...) {
        configSock->close();
	throw;
    }
    return doc;
}

/* static */
xercesc::DOMDocument* DSMEngine::parseXMLConfigFile(const string& xmlFileName)
	throw(nidas::core::XMLException)
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"parsing: %s",xmlFileName.c_str());

    auto_ptr<XMLParser> parser(new XMLParser());
    // throws XMLException

    // If parsing a local file, turn on validation
    parser->setDOMValidation(true);
    parser->setDOMValidateIfSchema(true);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(true);
    parser->setXercesSchemaFullChecking(true);
    parser->setDOMDatatypeNormalization(false);
    parser->setXercesUserAdoptsDOMDocument(true);

    xercesc::DOMDocument* doc = parser->parse(xmlFileName);
    return doc;
}

void DSMEngine::initialize(xercesc::DOMDocument* projectDoc)
	throw(n_u::InvalidParameterException)
{
    _project = Project::getInstance();

    _project->fromDOMElement(projectDoc->getDocumentElement());
    // throws n_u::InvalidParameterException;
    if (_configFile.length() > 0)
	_project->setConfigName(_configFile);

    char hostname[256];
    gethostname(hostname,sizeof(hostname));

    const DSMConfig* dsm = 0;
    _dsmConfig = 0;
    int ndsms = 0;
    for ( SiteIterator si = _project->getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	for ( DSMConfigIterator di = site->getDSMConfigIterator(); di.hasNext(); ) {
	    dsm = di.next();
	    ndsms++;
	    if (dsm->getName() == string(hostname)) {
		_dsmConfig = const_cast<DSMConfig*>(dsm);
		n_u::Logger::getInstance()->log(LOG_INFO,
		    "DSMEngine: found <dsm> for %s",
		    hostname);
	        break;
	    }
	}
    }
    if (ndsms == 1) _dsmConfig = const_cast<DSMConfig*>(dsm);

    if (!_dsmConfig)
    	throw n_u::InvalidParameterException("dsm","no match for hostname",
		hostname);
}

void DSMEngine::openSensors() throw(n_u::IOException)
{
    _selector = new SensorHandler(_dsmConfig->getRemoteSerialSocketPort());
    /*
     * Can't use real-time FIFO priority in user space via
     * pthread_setschedparam under RTLinux. How ironic.
     * It causes ENOSPC on the RTLinux fifos.
     */
    if (!isRTLinux()) {
        n_u::Logger::getInstance()->log(LOG_INFO,
            "DSMEngine: !RTLinux, so setting RT priority");
         _selector->setRealTimeFIFOPriority(50);
    }

    _pipeline = new SamplePipeline();
    _pipeline->setRealTime(true);
    _pipeline->setRawSorterLength(_dsmConfig->getRawSorterLength());
    _pipeline->setProcSorterLength(_dsmConfig->getProcSorterLength());
    _pipeline->setRawHeapMax(_dsmConfig->getRawHeapMax());
    _pipeline->setProcHeapMax(_dsmConfig->getProcHeapMax());

    _pipeline->setKeepStats(false);

    /* Initialize pipeline with all expected SampleTags. */
    const list<DSMSensor*> sensors = _dsmConfig->getSensors();
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
        _pipeline->connect(sensor);
    }
    _selector->start();
    _dsmConfig->openSensors(_selector);
}

void DSMEngine::connectOutputs() throw(n_u::IOException)
{
    // request connection for outputs
    const list<SampleOutput*>& outputs = _dsmConfig->getOutputs();
    list<SampleOutput*>::const_iterator oi;

    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	DLOG(("DSMEngine requesting connection from SampleOutput '%s'.",
		     output->getName().c_str()));
        SampleSource* src;
        if (output->isRaw()) src = _pipeline->getRawSampleSource();
        else src = _pipeline->getProcessedSampleSource();
        output->addSourceSampleTags(src->getSampleTags());
        SampleOutputRequestThread::getInstance()->addConnectRequest(output,this,0);
    }
}

/* implementation of SampleConnectionRequester::connect(SampleOutput*) */
void DSMEngine::connect(SampleOutput* output) throw()
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"DSMEngine: connection from %s", output->getName().c_str());

    _outputMutex.lock();
    _outputSet.insert(output);
    _outputMutex.unlock();

    if (output->isRaw()) _pipeline->getRawSampleSource()->addSampleClient(output);
    else  _pipeline->getProcessedSampleSource()->addSampleClient(output);
}

/*
 * An output wants to disconnect: probably the remote dsm_server went
 * down, or a client disconnected.
 */
void DSMEngine::disconnect(SampleOutput* output) throw()
{
    // cerr << "DSMEngine::disconnect, output=" << output << endl;
    if (output->isRaw()) _pipeline->getRawSampleSource()->removeSampleClient(output);
    else  _pipeline->getProcessedSampleSource()->removeSampleClient(output);

    _outputMutex.lock();
    _outputSet.erase(output);
    _outputMutex.unlock();

    try {
	output->finish();
	output->close();
    }
    catch (const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "DSMEngine: error closing %s: %s",
	    	output->getName().c_str(),ioe.what());
    }

    SampleOutput* orig = output->getOriginal();

    if (output != orig)
       SampleOutputRequestThread::getInstance()->addDeleteRequest(output);

    int delay = orig->getResubmitDelaySecs();
    if (delay < 0) return;
    SampleOutputRequestThread::getInstance()->addConnectRequest(orig,this,delay);
}

void DSMEngine::closeOutputs() throw()
{
   SampleOutputRequestThread::getInstance()->clear();

    _outputMutex.lock();

    set<SampleOutput*>::const_iterator oi = _outputSet.begin();
    for ( ; oi != _outputSet.end(); ++oi) {
	SampleOutput* output = *oi;
        if (output->isRaw()) _pipeline->getRawSampleSource()->removeSampleClient(output);
        else  _pipeline->getProcessedSampleSource()->removeSampleClient(output);
	try {
            output->finish();
            output->close();
            SampleOutput* orig = output->getOriginal();
	    if (output != orig) delete output;
	}
	catch(const n_u::IOException& e) {
	    n_u::Logger::getInstance()->log(LOG_INFO,
		"%s: %s",output->getName().c_str(),e.what());
	}
    }
    _outputSet.clear();
    _outputMutex.unlock();

    if (_dsmConfig) {
	const list<SampleOutput*>& outputs = _dsmConfig->getOutputs();
	list<SampleOutput*>::const_iterator oi = outputs.begin();
	for ( ; oi != outputs.end(); ++oi) {
	    SampleOutput* output = *oi;
	    try {
		output->finish();
		output->close();	// DSMConfig will delete
	    }
	    catch(const n_u::IOException& e) {
		n_u::Logger::getInstance()->log(LOG_INFO,
		    "%s: %s",output->getName().c_str(),e.what());
	    }
	}
    }
}

bool DSMEngine::isRTLinux()
{
    if (_rtlinux >= 0) return _rtlinux > 0;

    ifstream modfile("/proc/modules");

    // check to see if rtl module is loaded.
    while (!modfile.eof() && !modfile.fail()) {
        string module;
        modfile >> module;
        if (module == "rtl") {
	    _rtlinux = 1;
	    return true;
	}
        modfile.ignore(std::numeric_limits<int>::max(),'\n');
    }
    _rtlinux = 0;
    return false;
}

void DSMEngine::connectProcessors() throw(n_u::IOException,n_u::InvalidParameterException)
{
    SensorIterator si = _dsmConfig->getSensorIterator();
    for (; si.hasNext(); ) {
        DSMSensor* sensor = si.next();
        sensor->init();
    }

    ProcessorIterator pi = _dsmConfig->getProcessorIterator();
    // establish connections for processors
    for ( ; pi.hasNext(); ) {
        SampleIOProcessor* proc = pi.next();
        proc->connect(_pipeline);
    }
}

void DSMEngine::disconnectProcessors() throw()
{
    if (_dsmConfig) {
        ProcessorIterator pi = _dsmConfig->getProcessorIterator();
        for ( ; pi.hasNext(); ) {
            SampleIOProcessor* proc = pi.next();
            proc->disconnect(_pipeline);
        }
    }
}

