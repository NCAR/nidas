/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/DSMServer.h>

#include <nidas/core/Site.h>

#include <nidas/core/DSMTime.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/DOMObjectFactory.h>
#include <nidas/core/Version.h>

#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/Logger.h>
#include <nidas/util/McSocket.h>
#include <nidas/util/Process.h>

#include <unistd.h>
#include <memory> // auto_ptr<>
#include <pwd.h>

#ifdef HAS_CAPABILITY_H 
#include <sys/capability.h>
#include <sys/prctl.h>
#endif

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

/* static */
DSMServer* DSMServer::serverInstance = 0;

/* static */
string DSMServer::xmlFileName;

/* static */
bool DSMServer::quit = false;

/* static */
bool DSMServer::restart = false;

bool DSMServer::debug = false;

/* static */
DSMServerStat* DSMServer::_statusThread = 0;

/* static */
DSMServerIntf* DSMServer::_xmlrpcThread = 0;

/* static */
const char *DSMServer::rafXML = "$PROJ_DIR/projects/$PROJECT/$AIRCRAFT/nidas/flights.xml";

/* static */
const char *DSMServer::isffXML = "$ISFF/projects/$PROJECT/ISFF/config/configs.xml";

/* static */
string DSMServer::configsXMLName;

/* static */
string DSMServer::_username;

/* static */
uid_t DSMServer::_userid = 0;

/* static */
gid_t DSMServer::_groupid = 0;

/* static */
int DSMServer::main(int argc, char** argv) throw()
{

    int result = 0;
    if ((result = parseRunstring(argc,argv)) != 0) return result;

    n_u::Logger* logger = 0;
    n_u::LogConfig lc;

    if (debug) {
        logger = n_u::Logger::createInstance(&std::cerr);
        lc.level = n_u::LOGGER_DEBUG;
    }
    else {
	// fork to background, chdir to /,
        // send stdout/stderr to /dev/null
	if (daemon(0,0) < 0) {
	    n_u::IOException e("DSMServer","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
        logger = n_u::Logger::createInstance(
                "dsm_server",LOG_CONS,LOG_LOCAL5);
        lc.level = n_u::LOGGER_DEBUG;
    }
    logger->setScheme(n_u::LogScheme().addConfig (lc));

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

    gid_t gid = getGroupID();
    if (gid != 0 && getegid() != gid) {
        DLOG(("doing setgid(%d)",gid));
        if (setgid(gid) < 0)
            WLOG(("%s: cannot change group id to %d: %m","DSMServer",gid));
    }

    uid_t uid = getUserID();
    if (uid != 0 && geteuid() != uid) {
        DLOG(("doing setuid(%d=%s)",uid,getUserName().c_str()));
        if (setuid(uid) < 0)
            WLOG(("%s: cannot change userid to %d (%s): %m", "DSMServer",uid,getUserName().c_str()));
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
            logger->log(LOG_ERR,
                "dsm_server process, pid=%d is already running",pid);
            return 1;
        }
    }
    catch(const n_u::IOException& e) {
        logger->log(LOG_ERR,"dsm_server: %s",e.what());
        return 1;
    }

    setupSignals();

    while (!quit) {

        auto_ptr<Project> project;

	try {
	    if (configsXMLName.length() > 0) {
		ProjectConfigs configs;
		configs.parseXML(configsXMLName);
		// throws InvalidParameterException if no config for time
		const ProjectConfig* cfg = configs.getConfig(n_u::UTime());
		project.reset(cfg->getProject());
		xmlFileName = cfg->getXMLName();
	    }
	    else project.reset(parseXMLConfigFile(xmlFileName));
	}
	catch (const nidas::core::XMLException& e) {
	    logger->log(LOG_ERR,e.what());
	    result = 1;
	    break;
	}
	catch(const n_u::InvalidParameterException& e) {
	    logger->log(LOG_ERR,e.what());
	    result = 1;
	    break;
	}
	catch (const n_u::Exception& e) {
	    logger->log(LOG_ERR,e.what());
	    result = 1;
	    break;
	}
        project->setConfigName(xmlFileName);

	serverInstance = 0;

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
	    serverInstance = servers.front();
	}
	catch (const n_u::Exception& e) {
	    logger->log(LOG_ERR,e.what());
	    result = 1;
	    break;
	}


	try {
	    serverInstance->scheduleServices();
	}
	catch (const n_u::Exception& e) {
	    logger->log(LOG_ERR,e.what());
	}

        startStatusThread();
        startXmlRpcThread();

	serverInstance->waitOnServices();

        killStatusThread();
        killXmlRpcThread();

        // Project gets deleted here, which includes serverInstance.
    }

#ifdef DEBUG
    cerr << "XMLCachingParser::destroyInstance()" << endl;
#endif
    XMLCachingParser::destroyInstance();

#ifdef DEBUG
    cerr << "XMLImplementation::terminate()" << endl;
#endif
    XMLImplementation::terminate();

#ifdef DEBUG
    cerr << "clean return from main" << endl;
#endif
    return result;
}
                                                                                
/* static */
void DSMServer::startStatusThread() throw(n_u::Exception)
{
    _statusThread = DSMServerStat::getInstance();
    _statusThread->start();
}

// #define DEBUG
/* static */
void DSMServer::killStatusThread() throw(n_u::Exception)
{

    _statusThread->interrupt();
    // canceling statusThread results in segmentation fault.
    // There seems to be some incompatibility between
    // nanosleep and pthread_cancel.  So we just interrupt
    // it and have to wait up to a second for the join.
#define STATUS_THREAD_CANCEL
#ifdef STATUS_THREAD_CANCEL
    try {
#ifdef DEBUG
        cerr << "statusthread cancel, running=" <<
            _statusThread->isRunning() << endl;
#endif
        if (_statusThread->isRunning()) _statusThread->cancel();
    }
    catch(const n_u::Exception& e) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
        "statusThread: %s",e.what());
    }
#endif

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
// #undef DEBUG

/* static */
void DSMServer::startXmlRpcThread() throw(n_u::Exception)
{
    _xmlrpcThread = new DSMServerIntf();
    _xmlrpcThread->start();
}

/* static */
void DSMServer::killXmlRpcThread() throw(n_u::Exception)
{
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
}

/* static */
void DSMServer::setupSignals()
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
    act.sa_sigaction = DSMServer::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
    sigaction(SIGUSR1,&act,(struct sigaction *)0);
}
                                                                                
void DSMServer::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    n_u::Logger::getInstance()->log(LOG_INFO,
        "received signal %s(%d), si_signo=%d, si_errno=%d, si_code=%d",
        strsignal(sig),sig,
        (siginfo ? siginfo->si_signo : -1),
        (siginfo ? siginfo->si_errno : -1),
        (siginfo ? siginfo->si_code : -1));
                                                                                
    switch(sig) {
    case SIGHUP:
	DSMServer::restart = true;
	break;
    case SIGTERM:
    case SIGINT:
    case SIGUSR1:
	DSMServer::quit = true;
	break;
    }
}

/* static */
Project* DSMServer::parseXMLConfigFile(const string& xmlFileName)
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
    string expName = Project::expandEnvVars(xmlFileName);

    // This document belongs to the caching parser
    DOMDocument* doc = parser->parse(expName);
    // throws nidas::core::XMLException;

    Project* project = Project::getInstance();

    project->fromDOMElement(doc->getDocumentElement());
    // throws n_u::InvalidParameterException;

    return project;
}

DSMServer::DSMServer()
{
}

DSMServer::~DSMServer()
{

    // delete services. These are the configured services,
    // not the cloned copies.
    list<DSMService*>::const_iterator si;
#ifdef DEBUG
    cerr << "~DSMServer services.size=" << services.size() << endl;
#endif
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
#ifdef DEBUG
	cerr << "~DSMServer: deleting " << svc->getName() << endl;
#endif
	delete svc;
    }
#ifdef DEBUG
    cerr << "~DSMServer: deleted services " << endl;
#endif
}

DSMServiceIterator DSMServer::getDSMServiceIterator() const
{
    return DSMServiceIterator(this);
}

ProcessorIterator DSMServer::getProcessorIterator() const
{
    return ProcessorIterator(this);
}

SiteIterator DSMServer::getSiteIterator() const
{
    return SiteIterator(this);
}

DSMConfigIterator DSMServer::getDSMConfigIterator() const
{
    return DSMConfigIterator(this);
}

SensorIterator DSMServer::getSensorIterator() const
{
    return SensorIterator(this);
}

SampleTagIterator DSMServer::getSampleTagIterator() const
{
    return SampleTagIterator(this);
}

void DSMServer::fromDOMElement(const DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);

    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    const string& aname = attr.getName();
	    const string& aval = attr.getValue();
	    if (aname == "name") setName(aval);
	}
    }
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();
	if (elname == "service") {
	    const string classattr = DSMService::getClassName(
	    	(DOMElement*)child);
	    if (classattr.length() == 0) 
		throw n_u::InvalidParameterException(
		    "DSMServer::fromDOMElement",
			elname,
			"does not have a class attribute");
	    DOMable* domable;
	    try {
		domable = DOMObjectFactory::createObject(classattr);
	    }
	    catch (const n_u::Exception& e) {
		throw n_u::InvalidParameterException("service",
		    classattr,e.what());
	    }
	    DSMService* service = dynamic_cast<DSMService*>(domable);
	    if (!service) {
		delete domable;
		throw n_u::InvalidParameterException("service",
		    classattr,"is not of type DSMService");
	    }
	    service->setDSMServer(this);
	    service->fromDOMElement((DOMElement*)child);
	    addService(service);
	}
    }
}

void DSMServer::scheduleServices() throw(n_u::Exception)
{
    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;

	SiteIterator si = getSiteIterator();
	for ( ; si.hasNext(); ) {
	    const Site* site = si.next();
	    DSMConfigIterator di =
		site->getDSMConfigIterator();
	    for ( ; di.hasNext(); ) {
		const DSMConfig* dsm = di.next();
		Project::getInstance()->initSensors(dsm);
	    }
	}
	svc->schedule();
    }
}

void DSMServer::interruptServices() throw()
{
#ifdef DEBUG
    cerr << "interrupting services, size=" << services.size() << endl;
#endif
    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	// cerr << "doing interrupt on " << svc->getName() << endl;
	svc->interrupt();
    }
#ifdef DEBUG
    cerr << "interrupting services done" << endl;
#endif
}

void DSMServer::cancelServices() throw()
{
    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	// cerr << "doing cancel on " << svc->getName() << endl;
	svc->cancel();
    }
}

void DSMServer::joinServices() throw()
{

    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	// cerr << "doing join on " << svc->getName() << endl;
	svc->join();
	// cerr << svc->getName() << " joined" << endl;
    }

}

void DSMServer::waitOnServices() throw()
{

    // We wake up every so often and check our threads.
    // An INTR,HUP or TERM signal to this process will
    // cause nanosleep to return. sigAction sets quit or
    // restart, and then we break, and cancel our threads.
    struct timespec sleepTime;
    sleepTime.tv_sec = 10;
    sleepTime.tv_nsec = 0;

    for (;;) {

	int nservices = n_u::McSocketListener::check();

	list<DSMService*>::const_iterator si;
	for (si=services.begin(); si != services.end(); ++si) {
	    DSMService* svc = *si;
	    nservices += svc->checkSubThreads();
	}
	
#ifdef DEBUG
	cerr << "DSMServer::wait #services=" << nservices << endl;
#endif

	if (quit || restart) break;

        nanosleep(&sleepTime,0);

	if (quit || restart) break;
    }

#ifdef DEBUG
    cerr << "Break out of wait loop, interrupting services" << endl;
#endif

    interruptServices();	

#ifdef DEBUG
    cerr << "services interrupted" << endl;
#endif

    // wait a bit, then cancel whatever is still running
    sleepTime.tv_sec = 1;
    sleepTime.tv_nsec = 0;
    nanosleep(&sleepTime,0);

#ifdef DEBUG
    cerr << "cancelling services" << endl;
#endif
    cancelServices();	

#ifdef DEBUG
    cerr << "joining services" << endl;
#endif
    joinServices();
#ifdef DEBUG
    cerr << "services joined" << endl;
#endif

}

/* static */
int DSMServer::parseRunstring(int argc, char** argv)
{
    debug = false;
    extern char *optarg;	/* set by getopt() */
    extern int optind;		/* "  "     "     */
    int opt_char;		/* option character */
    while ((opt_char = getopt(argc, argv, "cdu:v")) != -1) {
        switch (opt_char) {
        case 'd':
            debug = true;
            break;
	case 'u':
            {
                struct passwd pwdbuf;
                struct passwd *result;
                long nb = sysconf(_SC_GETPW_R_SIZE_MAX);
                if (nb < 0) nb = 4096;
                char* strbuf = new char[nb];
                if (getpwnam_r(optarg,&pwdbuf,strbuf,nb,&result) < 0) {
                    cerr << "Unknown user: " << optarg << endl;
                    delete [] strbuf;
                    return usage(argv[0]);
                }
                _username = optarg;
                _userid = pwdbuf.pw_uid;
                _groupid = pwdbuf.pw_gid;
                delete [] strbuf;
            }
	    break;
	case 'v':
	    cout << Version::getSoftwareVersion() << endl;
	    return 1;
	    break;
        case 'c':
	    {
		const char* re = getenv("PROJ_DIR");
		const char* pe = getenv("PROJECT");
		const char* ae = getenv("AIRCRAFT");
		const char* ie = getenv("ISFF");
		if (re && pe && ae) configsXMLName = Project::expandEnvVars(rafXML);
		else if (ie && pe) configsXMLName = Project::expandEnvVars(isffXML);
		if (configsXMLName.length() == 0) {
		    cerr <<
			"Environment variables not set correctly to find XML file of project configurations." << endl;
		    cerr << "Cannot find " << rafXML << endl << "or " << isffXML << endl;
		    return usage(argv[0]);
		}
	    }
	    break;
        case '?':
            return usage(argv[0]);
        }
    }
    if (optind == argc - 1) xmlFileName = string(argv[optind++]);
    else if (configsXMLName.length() == 0) {
	const char* cfg = getenv("NIDAS_CONFIG");
	if (!cfg) {
	    cerr <<
		"Error: XML config file not found in runstring or $NIDAS_CONFIG" <<
	    endl;
            return usage(argv[0]);
        }
    	xmlFileName = cfg;
    }
    return 0;
}

/* static */
int DSMServer::usage(const char* argv0)
{
    const char* cfg;
    cerr << "\
Usage: " << argv0 << " [-c] [-d] [-u username] [-v] [config]\n\
  -c: read configs XML file to find current project configuration, either\n\t" << 
    rafXML << "\nor\n\t" << isffXML << "\n\
  -d: debug. Run in foreground and send messages to stderr with loglevel DEBUG.\n\
      Otherwise run in the background, cd to /, and log messages to syslog\n\
  -u username: after startup, switch userid to username from root\n\
  -v: display software version number and exit\n\
  config: (optional) name of DSM configuration file.\n\
    This parameter is not used if you specify the -c option\n\
    default: $NIDAS_CONFIG=\"" <<
	  	((cfg = getenv("NIDAS_CONFIG")) ? cfg : "<not set>") << "\"\n\
    Note: use an absolute path to this file if you run in the background without -d." << endl;
    return 1;
}

