/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-04-06 11:14:25 -0600 (Mon, 06 Apr 2009) $

    $LastChangedRevision: 4560 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/DSMServer.cc $
 ********************************************************************
*/

#include <nidas/core/DSMServerApp.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/DSMServerIntf.h>
#include <nidas/core/StatusThread.h>
#include <nidas/core/DSMService.h>
#include <nidas/core/Site.h>
#include <nidas/core/ProjectConfigs.h>
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/Version.h>

#include <nidas/util/Process.h>
#include <nidas/util/Logger.h>

#include <unistd.h>
#include <sys/param.h>  // MAXHOSTNAMELEN
#include <memory> // auto_ptr<>
#include <pwd.h>

#ifdef HAS_CAPABILITY_H 
#include <sys/capability.h>
#include <sys/prctl.h>
#endif

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

namespace {
    int defaultLogLevel = n_u::LOGGER_INFO;
};

/* static */
DSMServerApp* DSMServerApp::_instance = 0;

DSMServerApp::DSMServerApp() : _debug(false),_runState(RUN),
    _userid(0),_groupid(0),_xmlrpcThread(0),_statusThread(0),
    _externalControl(false),_logLevel(defaultLogLevel),
    _optionalProcessing(false)
{
    _rafXML = "$PROJ_DIR/$PROJECT/$AIRCRAFT/nidas/flights.xml";
    _isffXML = "$ISFF/projects/$PROJECT/ISFF/config/configs.xml";

    setupSignals();
}
DSMServerApp::~DSMServerApp()
{
    SampleOutputRequestThread::destroyInstance();
}

int DSMServerApp::parseRunstring(int argc, char** argv)
{
    int opt_char;		/* option character */
    while ((opt_char = getopt(argc, argv, "cdl:oru:v")) != -1) {
        switch (opt_char) {
        case 'd':
            _debug = true;
            _logLevel = n_u::LOGGER_DEBUG;
            break;
	case 'l':
            _logLevel = atoi(optarg);
	    break;
        case 'o':
            _optionalProcessing = true;
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
                int res;
                vector<char> strbuf(nb);
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
        case 'c':
	    {
                const char* cfg = getenv("NIDAS_CONFIGS");
                if (cfg) _configsXMLName = cfg;
                else {
                    const char* re = getenv("PROJ_DIR");
                    const char* pe = getenv("PROJECT");
                    const char* ae = getenv("AIRCRAFT");
                    const char* ie = getenv("ISFF");
                    if (re && pe && ae) _configsXMLName = n_u::Process::expandEnvVars(_rafXML);
                    else if (ie && pe) _configsXMLName = n_u::Process::expandEnvVars(_isffXML);
                }
                if (_configsXMLName.length() == 0) {
                    cerr <<
                        "Environment variables not set correctly to find XML file of project configurations." << endl;
                    cerr << "Cannot find " << _rafXML << endl << "or " << _isffXML << endl;
                    return usage(argv[0]);
                }
	    }
	    break;
        case '?':
            return usage(argv[0]);
        }
    }
    if (optind == argc - 1) _xmlFileName = string(argv[optind++]);
    else if (_configsXMLName.length() == 0) {
	const char* cfg = getenv("NIDAS_CONFIG");
	if (!cfg) {
	    cerr <<
		"Error: XML config file not found in runstring or $NIDAS_CONFIG" <<
	    endl;
            return usage(argv[0]);
        }
    	_xmlFileName = cfg;
    }
    return 0;
}

int DSMServerApp::usage(const char* argv0)
{
    const char* cfg;
    cerr << "\
Usage: " << argv0 << " [-c] [-d] [-l level] [-o] [-r] [-u username] [-v] [config]\n\
  -c: read configs XML file to find current project configuration, either\n\t" << 
    "\t$NIDAS_CONFIGS\nor\n\t" << _rafXML << "\nor\n\t" << _isffXML << "\n\
  -d: debug, run in foreground and send messages to stderr with log level of debug\n\
      Otherwise run in the background, cd to /, and log messages to syslog\n\
      Specify a -l option after -d to change the log level from debug\n\
  -l loglevel: set logging level, 7=debug,6=info,5=notice,4=warning,3=err,...\n\
     The default level if no -d option is " << defaultLogLevel << "\n\
  -o: run processors marked as optional in XML\n\
  -r: rpc, start XML RPC thread to respond to external commands\n\
  -u username: after startup, switch userid to username\n\
  -v: display software version number and exit\n\
  config: (optional) name of DSM configuration file.\n\
    This parameter is not used if you specify the -c option\n\
    default: $NIDAS_CONFIG=\"" <<
	  	((cfg = getenv("NIDAS_CONFIG")) ? cfg : "<not set>") << "\"\n\
    Note: use an absolute path to this file if you run in the background without -d." << endl;
    return 1;
}
/* static */
int DSMServerApp::main(int argc, char** argv) throw()
{
    DSMServerApp app;

    int res = 0;
    if ((res = app.parseRunstring(argc,argv)) != 0) return res;

    app.initLogger();

    if ((res = app.initProcess(argv[0])) != 0) return res;

    _instance = &app;

    try {

        // starts XMLRPC thread if -r runstring option
        app.startXmlRpcThread();

        res = app.run();

        app.killXmlRpcThread();
    }
    catch (const n_u::Exception &e) {
        PLOG(("%s",e.what()));
    }

    _instance = 0;

#ifdef DEBUG
    cerr << "XMLCachingParser::destroyInstance()" << endl;
#endif
    XMLCachingParser::destroyInstance();

#ifdef DEBUG
    cerr << "XMLImplementation::terminate()" << endl;
#endif
    XMLImplementation::terminate();

    return res;
}

void DSMServerApp::initLogger()
{
    n_u::LogConfig lc;
    n_u::Logger* logger = 0;
    lc.level = _logLevel;
    if (_debug) logger = n_u::Logger::createInstance(&std::cerr);
    else {
	// fork to background, chdir to /,
        // send stdout/stderr to /dev/null
	if (daemon(0,0) < 0) {
	    n_u::IOException e("DSMServer","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
        logger = n_u::Logger::createInstance(
                "dsm_server",LOG_CONS,LOG_LOCAL5);
    }
    logger->setScheme(n_u::LogScheme("dsm_server").addConfig (lc));
}

int DSMServerApp::initProcess(const char* argv0)
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
        n_u::Process::addEffectiveCapability(CAP_NET_ADMIN);
#ifdef DEBUG
        DLOG(("CAP_SYS_NICE = ") << n_u::Process::getEffectiveCapability(CAP_SYS_NICE));
        DLOG(("PR_GET_SECUREBITS=") << hex << prctl(PR_GET_SECUREBITS,0,0,0,0) << dec);
#endif
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s: %s",argv0,e.what()));
    }
    if (!n_u::Process::getEffectiveCapability(CAP_SYS_NICE))
        WLOG(("%s: CAP_SYS_NICE not in effect. Will not be able to use real-time priority",argv0));

#ifdef DEBUG
    DLOG(("CAP_SYS_NICE = ") << n_u::Process::getEffectiveCapability(CAP_SYS_NICE));
    DLOG(("PR_GET_SECUREBITS=") << hex << prctl(PR_GET_SECUREBITS,0,0,0,0) << dec);
#endif
#endif

    // Open and check the pid file after the above setuid() and daemon() calls.
    try {
        string pidname = "/var/run/nidas/dsm_server.pid";
        pid_t pid;
        try {
            pid = n_u::Process::checkPidFile(pidname);
        }
        catch(const n_u::IOException& e) {
            if (e.getErrno() == EACCES || e.getErrno() == ENOENT) {
                WLOG(("%s: %s. Will try placing the file on /tmp",pidname.c_str(),e.what()));
                pidname = "/tmp/dsm_server.pid";
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

    return 0;
}

int DSMServerApp::run() throw()
{
    int res = 0;
    SampleOutputRequestThread::getInstance()->start();
    for (; _runState != QUIT; ) {

        if (_runState == ERROR) {
            waitForSignal(15);
            if (_runState == QUIT) {
                res = 1;    // quit after an error. Return non-zero result
                break;
            }
            _runState = RUN;
        }

        Project project;

	try {
	    if (_configsXMLName.length() > 0) {
		ProjectConfigs configs;
		configs.parseXML(_configsXMLName);
		// throws InvalidParameterException if no config for time
		const ProjectConfig* cfg = configs.getConfig(n_u::UTime());
                cfg->initProject(project);
		_xmlFileName = cfg->getXMLName();
	    }
	    else parseXMLConfigFile(_xmlFileName,project);
	}
	catch (const nidas::core::XMLException& e) {
	    PLOG(("%s",e.what()));
	    _runState = ERROR;
            continue;
	}
	catch(const n_u::InvalidParameterException& e) {
	    PLOG(("%s",e.what()));
	    _runState = ERROR;
            continue;
	}
	catch (const n_u::Exception& e) {
	    PLOG(("%s",e.what()));
	    _runState = ERROR;
            continue;
	}

        project.setConfigName(_xmlFileName);

	DSMServer* server = 0;

	try {
	    char hostname[MAXHOSTNAMELEN];
	    gethostname(hostname,sizeof(hostname));

	    list<DSMServer*> servers =
	    	project.findServers(hostname);

	    if (servers.empty())
	    	throw n_u::InvalidParameterException("project","server",
			string("Can't find server entry for ") + hostname);
	    if (servers.size() > 1)
	    	throw n_u::InvalidParameterException("project","server",
			string("Multiple servers for ") + hostname);
	    server = servers.front();
	}
	catch (const n_u::Exception& e) {
	    PLOG(("%s",e.what()));
            _runState = ERROR;
            continue;
	}
        if (_xmlrpcThread) _xmlrpcThread->setDSMServer(server);

        server->setXMLConfigFileName(_xmlFileName);

	try {
	    server->scheduleServices(_optionalProcessing);
	}
	catch (const n_u::Exception& e) {
	    PLOG(("%s",e.what()));
            _runState = ERROR;
	}

        // start status thread if port is defined, via
        // <server statusAddr="sock::port"/>  in the configuration
        if (server->getStatusSocketAddr().getPort() != 0)
            startStatusThread(server);

        while (_runState == RUN) waitForSignal(0);

        killStatusThread();

        server->interruptServices();	

        server->joinServices();

        if (_xmlrpcThread) _xmlrpcThread->setDSMServer(0);

        // Project gets deleted here, which includes _server.
    }
    return res;
}

                                                                                
void DSMServerApp::startXmlRpcThread() throw(n_u::Exception)
{
    if (!_externalControl) return;
    if (_xmlrpcThread) return;
    _xmlrpcThread = new DSMServerIntf();
    _xmlrpcThread->start();
}

void DSMServerApp::killXmlRpcThread() throw()
{
    if (!_xmlrpcThread) return;
    try {
        if (_xmlrpcThread->isRunning()) {
            DLOG(("kill(SIGUSR1) xmlrpcThread"));
            _xmlrpcThread->kill(SIGUSR1);
        }
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s",e.what()));
    }
    try {
        DLOG(("joining xmlrpcThread"));
       _xmlrpcThread->join();
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s",e.what()));
    }
    delete _xmlrpcThread;
   _xmlrpcThread = 0;
}

void DSMServerApp::startStatusThread(DSMServer* server) throw(n_u::Exception)
{
    if (_statusThread) return;
    if (server->getStatusSocketAddr().getPort() != 0) {
        _statusThread = new DSMServerStat("DSMServerStat",server);
        _statusThread->start();
    }
}

void DSMServerApp::killStatusThread() throw()
{
    if (!_statusThread) return;

    try {
        if (_statusThread->isRunning()) {
            _statusThread->kill(SIGUSR1);
            DLOG(("kill(SIGUSR1) statusThread"));
        }
    }
    catch(const n_u::Exception& e) {
        WLOG(("statusThread: %s",e.what()));
    }
    try {
        DLOG(("joining statusThread"));
        _statusThread->join();
    }
    catch(const n_u::Exception& e) {
        WLOG(("statusThread: %s",e.what()));
    }
    delete _statusThread;
    _statusThread = 0;
}

void DSMServerApp::setupSignals()
{
    // block all signals, except some that indicate things
    // are amiss, and trace/breakpoint/profiling signals.
    sigfillset(&_signalMask);
    sigdelset(&_signalMask,SIGFPE);
    sigdelset(&_signalMask,SIGILL);
    sigdelset(&_signalMask,SIGBUS);
    sigdelset(&_signalMask,SIGSEGV);
    sigdelset(&_signalMask,SIGXCPU);
    sigdelset(&_signalMask,SIGXFSZ);
    sigdelset(&_signalMask,SIGTRAP);
    sigdelset(&_signalMask,SIGPROF);
    pthread_sigmask(SIG_BLOCK,&_signalMask,0);

    // unblock these in waitForSignal
    sigemptyset(&_signalMask);
    sigaddset(&_signalMask,SIGUSR2);
    sigaddset(&_signalMask,SIGHUP);
    sigaddset(&_signalMask,SIGTERM);
    sigaddset(&_signalMask,SIGINT);
}

void DSMServerApp::waitForSignal(int timeoutSecs)
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

    switch(sig) {
    case SIGHUP:
	_runState = RESTART;
	break;
    case SIGTERM:
    case SIGINT:
	_runState = QUIT;
        break;
    case SIGUSR2:
        // an XMLRPC method could set _runState and send SIGUSR2
	break;
    default:
        WLOG(("sigtimedwait unknown signal:") << strsignal(sig));
        break;
    }
}

void DSMServerApp::parseXMLConfigFile(const string& xmlFileName,Project& project)
        throw(nidas::core::XMLException,n_u::InvalidParameterException,n_u::IOException)
{
    XMLCachingParser* parser = XMLCachingParser::getInstance();
    // throws nidas::core::XMLException

    // If parsing a local file, turn on validation
    parser->setDOMValidation(true);
    parser->setDOMValidateIfSchema(true);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(true);
    parser->setXercesSchemaFullChecking(true);
    parser->setDOMDatatypeNormalization(false);

    // expand environment variables in name
    string expName = n_u::Process::expandEnvVars(xmlFileName);

    // Do not doc->release() this DOMDocument since it is
    // owned by the caching parser.
    NLOG(("parsing: ") << expName);
    xercesc::DOMDocument* doc = parser->parse(expName);
    // throws nidas::core::XMLException;

    project.fromDOMElement(doc->getDocumentElement());
    // throws n_u::InvalidParameterException;
}

