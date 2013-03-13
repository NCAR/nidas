// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
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

#include <nidas/core/DSMEngine.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/StatusThread.h>
#include <nidas/core/DSMEngineIntf.h>
#include <nidas/core/DerivedDataReader.h>
#include <nidas/core/SensorHandler.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/core/requestXMLConfig.h>

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

#include <unistd.h>  // for getopt(), optind, optarg

#ifdef HAS_CAPABILITY_H 
#include <sys/prctl.h>
#endif 

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;
namespace n_c = nidas::core;

namespace {
    int defaultLogLevel = n_u::LOGGER_NOTICE;
};

/* static */
DSMEngine* DSMEngine::_instance = 0;

DSMEngine::DSMEngine():
    _externalControl(false),_runState(DSM_RUNNING),_command(DSM_RUN),
    _syslogit(true),_configFile(),_configSockAddr(),
    _project(0), _dsmConfig(0),_selector(0),_pipeline(0),
    _statusThread(0),_xmlrpcThread(0),
    _outputSet(),_outputMutex(),
    _username(),_userid(0),_groupid(0),
    _logLevel(defaultLogLevel),_signalMask(),_myThreadId(::pthread_self())
{
    try {
	_configSockAddr = n_u::Inet4SocketAddress(
	    n_u::Inet4Address::getByName(NIDAS_MULTICAST_ADDR),
	    NIDAS_SVC_REQUEST_PORT_UDP);
    }
    catch(const n_u::UnknownHostException& e) {	// shouldn't happen
        cerr << e.what();
    }
    setupSignals();
}

DSMEngine::~DSMEngine()
{
    delete _statusThread;
    delete _xmlrpcThread;
    SampleOutputRequestThread::destroyInstance();
    delete _project;
    _project = 0;
    SamplePools::deleteInstance();
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

    // If the user has not selected -d (debug), initLogger will fork
    // to the background, using daemon(). After the fork, threads other than this
    // main thread will not be running, unless they use pthread_atfork().
    // So, in general, don't start any threads before this.
    engine.initLogger();

    if ((res = engine.initProcess(argv[0])) != 0) return res;

    long minflts,majflts,nswap;
    getPageFaults(minflts,majflts,nswap);

    // Set the singleton instance
    _instance = &engine;

    try {
        engine.startXmlRpcThread();

        res = engine.run();		// doesn't throw exceptions

        engine.killXmlRpcThread();
    }
    catch (const n_u::Exception &e) {
        PLOG(("%s",e.what()));
    }

    // Should figure out how to delete this automagically.
    DSMSensor::deleteLooper();

    XMLImplementation::terminate();

    SamplePoolInterface* charPool = SamplePool<SampleT<char> >::getInstance();
    ILOG(("dsm: sample pools: #s%d,#m%d,#l%d,#o%d\n",
                    charPool->getNSmallSamplesIn(),
                    charPool->getNMediumSamplesIn(),
                    charPool->getNLargeSamplesIn(),
                    charPool->getNSamplesOut()));

    logPageFaultDiffs(minflts,majflts,nswap);

    if (engine.getCommand() == DSM_SHUTDOWN) n_u::Process::spawn("halt");
    else if (engine.getCommand() == DSM_REBOOT) n_u::Process::spawn("reboot");

    // All users of singleton instance should have been shut down.
    _instance = 0;

    // Hack: wait for detached threads to delete themselves, so that
    // valgrind --leak-check=full doesn't complain.
    {
        struct timespec slp = {0, NSECS_PER_SEC / 50};
        nanosleep(&slp,0);
    }

    return res;
}

int DSMEngine::parseRunstring(int argc, char** argv) throw()
{
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
                int res;
                if ((res = getpwnam_r(optarg,&pwdbuf,&strbuf.front(),nb,&result)) != 0) {
                    cerr << "getpwnam_r: " << n_u::Exception::errnoToString(res) << endl;
                    return 1;
                }
                else if (result == 0) {
                    cerr << "Unknown user: " << optarg << endl;
                    return 1;
                }
                _username = optarg;
                _userid = pwdbuf.pw_uid;
                _groupid = pwdbuf.pw_gid;
            }
	    break;
	case 'v':
	    cout << Version::getSoftwareVersion() << endl;
	    return 1;
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
  -u user: switch user id to given user after setting required capabilities\n\
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
    n_u::LogScheme logscheme("dsm");
    lc.level = _logLevel;
    if (_syslogit) {
	// fork to background
	if (daemon(0,0) < 0) {
	    n_u::IOException e("DSMEngine","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
	logger = n_u::Logger::createInstance("dsm",LOG_PID,LOG_LOCAL5);
        logscheme.setShowFields("level,message");
    }
    else
    {
	logger = n_u::Logger::createInstance(&std::cerr);
    }
    logscheme.addConfig(lc);
    logger->setScheme(logscheme);
}

int DSMEngine::initProcess(const char* argv0)
{

#ifdef HAS_CAPABILITY_H 
    /* man 7 capabilities:
     * If a thread that has a 0 value for one or more of its user IDs wants to
     * prevent its permitted capability set being cleared when it  resets  all
     * of  its  user  IDs  to  non-zero values, it can do so using the prctl()
     * PR_SET_KEEPCAPS operation.
     *
     * If we are started as uid=0 from sudo, and then setuid(x) below
     * we want to keep our permitted capabilities.
     */
    try {
	if (prctl(PR_SET_KEEPCAPS,1,0,0,0) < 0)
	    throw n_u::Exception("prctl(PR_SET_KEEPCAPS,1)",errno);
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s: %s. Will not be able to use real-time priority",argv0,e.what()));
    }
#endif

    gid_t gid = getGroupID();
    if (gid != 0 && getegid() != gid) {
        DLOG(("doing setgid(%d)",gid));
        if (setgid(gid) < 0)
            WLOG(("%s: cannot change group id to %d: %m",argv0,gid));
    }

    uid_t uid = getUserID();
    if (uid != 0 && geteuid() != uid) {
        DLOG(("doing setuid(%d=%s)",uid,getUserName().c_str()));
        if (setuid(uid) < 0)
            WLOG(("%s: cannot change userid to %d (%s): %m", argv0,
                uid,getUserName().c_str()));
    }

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
        WLOG(("%s: %s. Will not be able to use real-time priority",argv0,e.what()));
    }
#endif

    // Open and check the pid file after the above daemon() call.
    try {
        string pidname = "/var/run/nidas/dsm.pid";
        pid_t pid;
        try {
            pid = n_u::Process::checkPidFile(pidname);
        }
        catch(const n_u::IOException& e) {
            if (e.getErrno() == EACCES || e.getErrno() == ENOENT) {
                WLOG(("%s: %s. Will try placing the file on /tmp",pidname.c_str(),e.what()));
                pidname = "/tmp/dsm.pid";
                pid = n_u::Process::checkPidFile(pidname);
            }
            else throw;
        }
        if (pid > 0) {
            PLOG(("%s: pid=%d is already running",argv0,pid));
            return 1;
        }
    }
    catch(const n_u::IOException& e) {
        PLOG(("%s: %s",argv0,e.what()));
        return 1;
    }

#ifdef DO_MLOCKALL
    ILOG(("Locking memory: mlockall(MCL_CURRENT | MCL_FUTURE)"));
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        n_u::IOException e(argv0,"mlockall",errno);
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s",e.what());
    }
#endif
    return 0;
}

int DSMEngine::run() throw()
{
    xercesc::DOMDocument* projectDoc = 0;

    SampleOutputRequestThread::getInstance()->start();

    int res = 0;

    for (; !quitCommand(_command); ) {

        // cleanup before re-starting the loop
        joinDataThreads();

        disconnectProcessors();

        closeOutputs();

        if (projectDoc) {
            projectDoc->release();
            projectDoc = 0;
        }

        delete _project;
        _project = 0;
        _dsmConfig = 0;

        // One of the data threads is the SensorHandler. Deleting the SensorHandler
        // deletes all the sensors. They should be deleted after the
        // project is deleted since various objects in the project may
        // hold references to the sensors.
        deleteDataThreads();

        while (_command == DSM_STOP) waitForSignal(0);

        if (_runState == DSM_ERROR && !quitCommand(_command)) {
            waitForSignal(15);
            if (quitCommand(_command)) {
                res = 1;    // quit after an error. Return non-zero result
                break;
            }
            _command = DSM_RUN;
        }

        
        if (_command == DSM_RESTART) _command = DSM_RUN;
        if (_command != DSM_RUN) continue;

        // first fetch the configuration
        try {
            if (_configFile.length() == 0) {
                projectDoc = n_c::requestXMLConfig(false,_configSockAddr, &_signalMask);
            }
            else {
                // expand environment variables in name
                string expName = n_u::Process::expandEnvVars(_configFile);
                projectDoc = parseXMLConfigFile(expName);
            }
        }
        catch (const XMLException& e) {
            PLOG(("%s",e.what()));
            _runState = DSM_ERROR;
            continue;
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
            _runState = DSM_ERROR;
            continue;
        }

        if (_command != DSM_RUN) continue;

        // then initialize the DSMEngine
        try {
            initialize(projectDoc);
        }
        catch (const n_u::InvalidParameterException& e) {
            PLOG(("%s",e.what()));
            _runState = DSM_ERROR;
            continue;
        }
        projectDoc->release();
        projectDoc = 0;
        XMLImplementation::terminate();

        res = 0;

        if (_dsmConfig->getDerivedDataSocketAddr().getPort() != 0) {
              DerivedDataReader::createInstance(_dsmConfig->getDerivedDataSocketAddr());
	}
        // start your sensors
        try {
            openSensors();
            connectOutputs();
            connectProcessors();
        }
        catch (const n_u::IOException& e) {
            PLOG(("%s",e.what()));
            _runState = DSM_ERROR;
            continue;
        }
        catch (const n_u::InvalidParameterException& e) {
            PLOG(("%s",e.what()));
            _runState = DSM_ERROR;
            continue;
        }
        if (_command != DSM_RUN) continue;

        // start the status Thread
        if (_dsmConfig->getStatusSocketAddr().getPort() != 0) {
            _statusThread = new DSMEngineStat("DSMEngineStat",_dsmConfig->getStatusSocketAddr());
            _statusThread->start();
	}
        _runState = DSM_RUNNING;

        while (_command == DSM_RUN) {
            waitForSignal(0);
        }

        if (quitCommand(_command)) break;
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

    delete _project;
    _project = 0;
    _dsmConfig = 0;

    deleteDataThreads();

    return res;
}

void DSMEngine::interrupt()
{
    if (_statusThread) _statusThread->interrupt();
    if (DerivedDataReader::getInstance()) DerivedDataReader::getInstance()->interrupt();
    if (_selector) _selector->interrupt();
    if (_pipeline) {
        _pipeline->flush();
        _pipeline->interrupt();
    }
}

void DSMEngine::joinDataThreads() throw()
{
    // stop/join the status thread before closing sensors.
    // The status thread also loops over sensors.
    if (_statusThread) {
        try {
            if (_statusThread->isRunning()) _statusThread->kill(SIGUSR1);
        }
        catch (const n_u::Exception& e) {
            WLOG(("%s",e.what()));
        }
        try {
            DLOG(("DSMEngine joining statusThread"));
            _statusThread->join();
            DLOG(("DSMEngine xmlrpcThread joined"));
        }
        catch (const n_u::Exception& e) {
            WLOG(("%s",e.what()));
        }
    }

    if (_selector) {
        try {
            DLOG(("DSMEngine joining selector"));
            _selector->join();
            DLOG(("DSMEngine selector joined"));
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
        }
    }

    if (_pipeline) {
        try {
            DLOG(("DSMEngine joining pipeline"));
            _pipeline->join();
            DLOG(("DSMEngine pipeline joined"));
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
        }
    }

    if (DerivedDataReader::getInstance()) {
        // If clients of DerivedDataReader are doing something that
        // blocks, it might still be running.
        try {
            for (int i = 0; i < 10; i++) {
                if (!DerivedDataReader::getInstance()->isRunning()) break;
                usleep(USECS_PER_SEC/10);
            }
            if (DerivedDataReader::getInstance()->isRunning()) {
                PLOG(("DerivedDataReader is still running, cancelling"));
                DerivedDataReader::getInstance()->cancel();
            }
        }
        catch (const n_u::Exception& e) {
            WLOG(("%s",e.what()));
        }

        try {
            DLOG(("DSMEngine joining DerivedDataReader"));
            DerivedDataReader::getInstance()->join();
            DLOG(("DSMEngine DerivedDataReader joined"));
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
        }
    }
}

void DSMEngine::deleteDataThreads() throw()
{
    delete _statusThread;
    _statusThread = 0;

    delete _selector;	// this closes any still-open sensors
    _selector = 0;

    if (DerivedDataReader::getInstance()) {
        DerivedDataReader::deleteInstance();
    }

    delete _pipeline;
    _pipeline = 0;
}

void DSMEngine::start()
{
    _command = DSM_RUN;
    pthread_kill(_myThreadId,SIGUSR1);
}

/*
 * Stop data acquisition threads and wait for next command.
 */
void DSMEngine::stop()
{
    _command = DSM_STOP;
    pthread_kill(_myThreadId,SIGUSR1);
}

void DSMEngine::restart()
{
    pthread_kill(_myThreadId,SIGHUP);
}

void DSMEngine::quit()
{
    pthread_kill(_myThreadId,SIGTERM);
}

void DSMEngine::shutdown()
{
    _command = DSM_SHUTDOWN;
    pthread_kill(_myThreadId,SIGUSR1);
}

void DSMEngine::reboot()
{
    _command = DSM_REBOOT;
    pthread_kill(_myThreadId,SIGUSR1);
}

void DSMEngine::setupSignals()
{
    // unblock these with sigwaitinfo/sigtimedwait in waitForSignal
    sigemptyset(&_signalMask);
    sigaddset(&_signalMask,SIGUSR1);
    sigaddset(&_signalMask,SIGHUP);
    sigaddset(&_signalMask,SIGTERM);
    sigaddset(&_signalMask,SIGINT);

    // block them otherwise
    pthread_sigmask(SIG_BLOCK,&_signalMask,0);
}

void DSMEngine::waitForSignal(int timeoutSecs)
{
    // pause, unblocking the signals I'm interested in
    int sig;
    if (timeoutSecs > 0) {
        struct timespec ts = {timeoutSecs,0};
        sig = sigtimedwait(&_signalMask,0,&ts);
    }
    else sig = sigwaitinfo(&_signalMask,0);

    if (sig < 0) {
        if (errno == EAGAIN) return;    // timeout
        // if errno == EINTR, then the wait was interrupted by a signal other
        // than those that are unblocked here in _signalMask. This 
        // must have been an unblocked and non-ignored signal.
        if (errno == EINTR) PLOG(("DSMEngine::waitForSignal(): unexpected signal"));
        else PLOG(("DSMEngine::waitForSignal(): ") << n_u::Exception::errnoToString(errno));
        return;
    }

    ILOG(("DSMEngine received signal ") << strsignal(sig) << '(' << sig << ')');
    switch(sig) {
    case SIGHUP:
	_command = DSM_RESTART;
	break;
    case SIGTERM:
    case SIGINT:
	_command = DSM_QUIT;
        break;
    case SIGUSR1:
        // an XMLRPC method could set _command and send SIGUSR1
	break;
    default:
        WLOG(("DSMEngine received unknown signal:") << strsignal(sig));
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
    if (!_xmlrpcThread) return;
    try {
        if (_xmlrpcThread->isRunning()) {
            _xmlrpcThread->kill(SIGUSR1);
        }
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s",e.what()));
    }
    try {
        DLOG(("DSMEnging joining xmlrpcThread"));
       _xmlrpcThread->join();
        DLOG(("DSMEnging xmlrpcThread joined"));
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s",e.what()));
    }

    delete _xmlrpcThread;
   _xmlrpcThread = 0;
}

void DSMEngine::registerSensorWithXmlRpc(const std::string& devname,DSMSensor* sensor)
{
    if (_xmlrpcThread) return _xmlrpcThread->registerSensor(devname,sensor);
}

void DSMEngine::initialize(xercesc::DOMDocument* projectDoc)
	throw(n_u::InvalidParameterException)
{
    _project = new Project();

    _project->fromDOMElement(projectDoc->getDocumentElement());
    // throws n_u::InvalidParameterException;
    if (_configFile.length() > 0)
	_project->setConfigName(_configFile);

    char hostnamechr[256];
    gethostname(hostnamechr,sizeof(hostnamechr));

    string hostname(hostnamechr);

    // location of first dot of hostname, or if not found 
    // the host string match will be done against entire hostname
    string::size_type dot = hostname.find('.');

    const DSMConfig* dsm = 0;
    _dsmConfig = 0;
    int ndsms = 0;
    for ( DSMConfigIterator di = _project->getDSMConfigIterator(); di.hasNext(); ) {
        dsm = di.next();
        ndsms++;
        if (dsm->getName() == hostname || dsm->getName() == hostname.substr(0,dot)) {
            _dsmConfig = const_cast<DSMConfig*>(dsm);
            n_u::Logger::getInstance()->log(LOG_INFO,
                "DSMEngine: found <dsm> for %s",
                hostname.c_str());
            break;
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

    n_u::Logger::getInstance()->log(LOG_INFO,"DSMEngine: setting RT priority");
    _selector->setRealTimeFIFOPriority(50);

    _pipeline = new SamplePipeline();
    _pipeline->setRealTime(true);
    _pipeline->setRawSorterLength(_dsmConfig->getRawSorterLength());
    _pipeline->setProcSorterLength(_dsmConfig->getProcSorterLength());
    _pipeline->setRawHeapMax(_dsmConfig->getRawHeapMax());
    _pipeline->setProcHeapMax(_dsmConfig->getProcHeapMax());
    _pipeline->setRawLateSampleCacheSize(_dsmConfig->getRawLateSampleCacheSize());
    _pipeline->setProcLateSampleCacheSize(_dsmConfig->getProcLateSampleCacheSize());

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
    if (output->isRaw()) _pipeline->getRawSampleSource()->removeSampleClient(output);
    else  _pipeline->getProcessedSampleSource()->removeSampleClient(output);

    _outputMutex.lock();
    _outputSet.erase(output);
    _outputMutex.unlock();

    output->flush();
    try {
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

    int delay = orig->getReconnectDelaySecs();
    if (delay < 0) return;
    SampleOutputRequestThread::getInstance()->addConnectRequest(orig,this,delay);
}

void DSMEngine::closeOutputs() throw()
{

    _outputMutex.lock();

    set<SampleOutput*>::const_iterator oi = _outputSet.begin();
    for ( ; oi != _outputSet.end(); ++oi) {
	SampleOutput* output = *oi;
        if (output->isRaw()) _pipeline->getRawSampleSource()->removeSampleClient(output);
        else  _pipeline->getProcessedSampleSource()->removeSampleClient(output);
        output->flush();
	try {
            output->close();
	}
	catch(const n_u::IOException& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,
		"%s: %s",output->getName().c_str(),e.what());
	}
        SampleOutput* orig = output->getOriginal();
        if (output != orig) delete output;
    }
    _outputSet.clear();
    _outputMutex.unlock();

    if (_dsmConfig) {
	const list<SampleOutput*>& outputs = _dsmConfig->getOutputs();
	list<SampleOutput*>::const_iterator oi = outputs.begin();
	for ( ; oi != outputs.end(); ++oi) {
	    SampleOutput* output = *oi;
            output->flush();
	    try {
		output->close();	// DSMConfig will delete
	    }
	    catch(const n_u::IOException& e) {
		n_u::Logger::getInstance()->log(LOG_ERR,
		    "%s: %s",output->getName().c_str(),e.what());
	    }
	}
    }
    SampleOutputRequestThread::getInstance()->clear();
}

void DSMEngine::connectProcessors() throw(n_u::IOException,n_u::InvalidParameterException)
{
    ProcessorIterator pi = _dsmConfig->getProcessorIterator();

    // If there are one or more processors defined, call sensor init methods.
    if (pi.hasNext()) {
        SensorIterator si = _dsmConfig->getSensorIterator();
        for (; si.hasNext(); ) {
            DSMSensor* sensor = si.next();
            sensor->init();
        }
    }

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
            proc->flush();
        }
    }
}

