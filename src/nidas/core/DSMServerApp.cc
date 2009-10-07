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

#include <nidas/util/Process.h>
#include <nidas/core/Version.h>
#include <nidas/core/SampleOutputRequestThread.h>

#include <unistd.h>
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
    int defaultLogLevel = n_u::LOGGER_NOTICE;
};

/* static */
DSMServerApp* DSMServerApp::_instance = 0;

DSMServerApp::DSMServerApp() : _debug(false),_runState(RUN),
    _userid(0),_groupid(0),_xmlrpcThread(0),_statusThread(0),
    _externalControl(false),_logLevel(defaultLogLevel)
{
    _rafXML = "$PROJ_DIR/$PROJECT/$AIRCRAFT/nidas/flights.xml";
    _isffXML = "$ISFF/projects/$PROJECT/ISFF/config/configs.xml";

}
DSMServerApp::~DSMServerApp()
{
    SampleOutputRequestThread::destroyInstance();
}

int DSMServerApp::parseRunstring(int argc, char** argv)
{
    extern char *optarg;	/* set by getopt() */
    extern int optind;		/* "  "     "     */
    int opt_char;		/* option character */
    while ((opt_char = getopt(argc, argv, "cdl:ru:v")) != -1) {
        switch (opt_char) {
        case 'd':
            _debug = true;
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
                    return usage(argv[0]);
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
Usage: " << argv0 << " [-c] [-d] [-l level] [-r] [-u username] [-v] [config]\n\
  -c: read configs XML file to find current project configuration, either\n\t" << 
    "\t$NIDAS_CONFIGS\nor\n\t" << _rafXML << "\nor\n\t" << _isffXML << "\n\
  -d: debug, run in foreground and send messages to stderr with log level of debug\n\
      Otherwise run in the background, cd to /, and log messages to syslog\n\
      Specify a -l option after -d to change the log level from debug\n\
  -l loglevel: set logging level, 7=debug,6=info,5=notice,4=warning,3=err,...\n\
     The default level if no -d option is " << defaultLogLevel << "\n\
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

#ifdef CAP_SYS_NICE
    try {
        n_u::Process::addEffectiveCapability(CAP_SYS_NICE);
#ifdef DEBUG
        DLOG(("CAP_SYS_NICE = ") << n_u::Process::getEffectiveCapability(CAP_SYS_NICE));
        DLOG(("PR_GET_SECUREBITS=") << hex << prctl(PR_GET_SECUREBITS,0,0,0,0) << dec);
#endif
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s: %s. Will not be able to use real-time priority",argv[0],e.what()));
    }
#endif

    gid_t gid = app.getGroupID();
    if (gid != 0 && getegid() != gid) {
        DLOG(("doing setgid(%d)",gid));
        if (setgid(gid) < 0)
            WLOG(("%s: cannot change group id to %d: %m","DSMServer",gid));
    }

    uid_t uid = app.getUserID();
    if (uid != 0 && geteuid() != uid) {
        DLOG(("doing setuid(%d=%s)",uid,app.getUserName().c_str()));
        if (setuid(uid) < 0)
            WLOG(("%s: cannot change userid to %d (%s): %m", "DSMServer",
                uid,app.getUserName().c_str()));
    }

#ifdef CAP_SYS_NICE
    // Check that CAP_SYS_NICE is still in effect after setuid.
    if (!n_u::Process::getEffectiveCapability(CAP_SYS_NICE))
        WLOG(("%s: CAP_SYS_NICE not in effect. Will not be able to use real-time priority",argv[0]));

#ifdef DEBUG
    DLOG(("CAP_SYS_NICE = ") << n_u::Process::getEffectiveCapability(CAP_SYS_NICE));
    DLOG(("PR_GET_SECUREBITS=") << hex << prctl(PR_GET_SECUREBITS,0,0,0,0) << dec);
#endif
#endif

    // Open and check the pid file after the above setuid() and daemon() calls.
    try {
        pid_t pid = n_u::Process::checkPidFile("/tmp/dsm_server.pid");
        if (pid > 0) {
            CLOG(("dsm_server process, pid=%d is already running",pid));
            return 1;
        }
    }
    catch(const n_u::IOException& e) {
        CLOG(("dsm_server: %s",e.what()));
        return 1;
    }

    _instance = &app;

    setupSignals();

    try {

        // starts XMLRPC thread if -r runstring option
        app.startXmlRpcThread();

        res = app.run();

        app.killXmlRpcThread();
    }
    catch (const n_u::Exception &e) {
        PLOG(("%s",e.what()));
    }

    unsetupSignals();

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

int DSMServerApp::run() throw()
{
    int res = 0;
    SampleOutputRequestThread::getInstance()->start();
    for (;;) {
        _runCond.lock();
        if (_runState == RESTART) _runState = RUN;
        else if (_runState == ERROR) {
            _runCond.unlock();
            sleep(15);
            _runCond.lock();
        }

        if (_runState == QUIT) {
            _runCond.unlock();
            break;
        }
        _runCond.unlock();
        _runState = RUN;

        auto_ptr<Project> project;

	try {
	    if (_configsXMLName.length() > 0) {
		ProjectConfigs configs;
		configs.parseXML(_configsXMLName);
		// throws InvalidParameterException if no config for time
		const ProjectConfig* cfg = configs.getConfig(n_u::UTime());
		project.reset(cfg->getProject());
		_xmlFileName = cfg->getXMLName();
	    }
	    else project.reset(parseXMLConfigFile(_xmlFileName));
	}
	catch (const nidas::core::XMLException& e) {
	    CLOG(("%s",e.what()));
            _runCond.lock();
	    if (_runState != QUIT) _runState = ERROR;
	    _runCond.unlock();
            continue;
	}
	catch(const n_u::InvalidParameterException& e) {
	cerr << "Invalid parameter exc, runState=" << _runState << endl;
	    CLOG(("%s",e.what()));
            _runCond.lock();
	    if (_runState != QUIT) _runState = ERROR;
	    _runCond.unlock();
            continue;
	}
	catch (const n_u::Exception& e) {
	    CLOG(("%s",e.what()));
            _runCond.lock();
	    if (_runState != QUIT) _runState = ERROR;
	    _runCond.unlock();
            continue;
	}
        if (_runState == QUIT) break;
        project->setConfigName(_xmlFileName);

	DSMServer* server = 0;

	try {
	    char hostname[MAXHOSTNAMELEN];
	    gethostname(hostname,sizeof(hostname));

	    list<DSMServer*> servers =
	    	Project::getInstance()->findServers(hostname);

	    if (servers.size() == 0)
	    	throw n_u::InvalidParameterException("project","server",
			string("Can't find server entry for ") + hostname);
	    if (servers.size() > 1)
	    	throw n_u::InvalidParameterException("project","server",
			string("Multiple servers for ") + hostname);
	    server = servers.front();
	}
	catch (const n_u::Exception& e) {
	    CLOG(("%s",e.what()));
            _runState = ERROR;
            continue;
	}

        server->setXMLConfigFileName(_xmlFileName);

	try {
	    server->scheduleServices();
	}
	catch (const n_u::Exception& e) {
	    PLOG(("%s",e.what()));
            _runState = ERROR;
	}

        // start status thread if port is defined, via
        // <server statusAddr="sock::port"/>  in the configuration
        if (server->getStatusSocketAddr().getPort() != 0)
            startStatusThread(server);

        _runCond.lock();
        while (_runState == RUN) _runCond.wait();
        _runCond.unlock();

        killStatusThread();

        server->interruptServices();	

        server->joinServices();

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
    _xmlrpcThread->interrupt();

#define XMLRPC_THREAD_CANCEL
#ifdef XMLRPC_THREAD_CANCEL
    // if we're using XmlRpcServer::work(-1.0) we must
    // do a cancel here.
    try {
        if (_xmlrpcThread->isRunning()) _xmlrpcThread->cancel();
    }
    catch(const n_u::Exception& e) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
        "xmlRpcThread: %s",e.what());
    }
#endif

#ifdef DEBUG
    cerr << "xmlrpcthread join" << endl;
#endif
    try {
        _xmlrpcThread->join();
    }
    catch(const n_u::Exception& e) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
        "xmlRpcThread: %s",e.what());
    }
    delete _xmlrpcThread;
    _xmlrpcThread = 0;
#ifdef DEBUG
    cerr << "xmlrpcthread joined" << endl;
#endif
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

    _statusThread->interrupt();

    try {
#ifdef DEBUG
        cerr << "statusthread join" << endl;
#endif
        _statusThread->join();
    }
    catch(const n_u::Exception& e) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
        "statusThread: %s",e.what());
    }
#ifdef DEBUG
    cerr << "statusthread delete" << endl;
#endif
    delete _statusThread;
    _statusThread = 0;
}

/* static */
void DSMServerApp::setupSignals()
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
    act.sa_sigaction = sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
    sigaction(SIGUSR1,&act,(struct sigaction *)0);
}
                                                                                
/* static */
void DSMServerApp::unsetupSignals()
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
    act.sa_handler = SIG_IGN;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
    sigaction(SIGUSR1,&act,(struct sigaction *)0);
}
                                                                                
/* static */
void DSMServerApp::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    n_u::Logger::getInstance()->log(LOG_INFO,
        "received signal %s(%d), si_signo=%d, si_errno=%d, si_code=%d",
        strsignal(sig),sig,
        (siginfo ? siginfo->si_signo : -1),
        (siginfo ? siginfo->si_errno : -1),
        (siginfo ? siginfo->si_code : -1));
                                                                                
    switch(sig) {
    case SIGHUP:
	DSMServerApp::getInstance()->interruptRestart();
	break;
    case SIGTERM:
    case SIGINT:
    case SIGUSR1:
	DSMServerApp::getInstance()->interruptQuit();
	break;
    }
}

void DSMServerApp::interruptQuit() throw()
{
    _runCond.lock();
    _runState = QUIT;
    _runCond.signal();
    _runCond.unlock();
}

void DSMServerApp::interruptRestart() throw()
{
    _runCond.lock();
    _runState = RESTART;
    _runCond.signal();
    _runCond.unlock();
}

Project* DSMServerApp::parseXMLConfigFile(const string& xmlFileName)
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
    parser->setXercesUserAdoptsDOMDocument(true);

    // expand environment variables in name
    string expName = n_u::Process::expandEnvVars(xmlFileName);

    // This document belongs to the caching parser
    xercesc::DOMDocument* doc = parser->parse(expName);
    // throws nidas::core::XMLException;

    Project* project = Project::getInstance();

    project->fromDOMElement(doc->getDocumentElement());
    // throws n_u::InvalidParameterException;

    return project;
}

