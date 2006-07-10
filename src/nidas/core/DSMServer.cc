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

#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/Logger.h>
#include <nidas/util/McSocket.h>

#include <unistd.h>

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
int DSMServer::main(int argc, char** argv) throw()
{

    int result = 0;
    if ((result = parseRunstring(argc,argv)) != 0) return result;

    n_u::Logger* logger = 0;

    if (debug) logger = n_u::Logger::createInstance(stderr);
    else {
	// fork to background, send stdout/stderr to /dev/null
	if (daemon(0,0) < 0) {
	    n_u::IOException e("DSMServer","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
        logger = n_u::Logger::createInstance(
                "dsm_server",LOG_CONS,LOG_LOCAL5);
    }

    setupSignals();

    startStatusThread();
    startXmlRpcThread();

    while (!quit) {

        Project* project = 0;
	try {
	    project = parseXMLConfigFile(xmlFileName);
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

	serverInstance = 0;

	try {
	    char hostname[MAXHOSTNAMELEN];
	    gethostname(hostname,sizeof(hostname));

	    serverInstance = Project::getInstance()->findServer(hostname);

	    if (!serverInstance)
	    	throw n_u::InvalidParameterException("project","server",
			string("Can't find server entry for ") + hostname);
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

	serverInstance->waitOnServices();

	delete project;
    }

    killStatusThread();
    killXmlRpcThread();

    // cerr << "XMLCachingParser::destroyInstance()" << endl;
    XMLCachingParser::destroyInstance();

    // cerr << "XMLImplementation::terminate()" << endl;
    XMLImplementation::terminate();
    return result;
}
                                                                                
/* static */
void DSMServer::startStatusThread() throw(n_u::Exception)
{
    _statusThread = DSMServerStat::getInstance();
    _statusThread->start();
}

/* static */
void DSMServer::killStatusThread() throw(n_u::Exception)
{
    cerr << "statusthread cancel" << endl;
    _statusThread->cancel();
    cerr << "statusthread join" << endl;
    _statusThread->join();
    cerr << "statusthread delete" << endl;
    delete _statusThread;
    _statusThread = 0;
}

/* static */
void DSMServer::startXmlRpcThread() throw(n_u::Exception)
{
    _xmlrpcThread = new DSMServerIntf();
    _xmlrpcThread->start();
}

/* static */
void DSMServer::killXmlRpcThread() throw(n_u::Exception)
{
    cerr << "xmlrpcthread cancel" << endl;
    _xmlrpcThread->cancel();
    cerr << "xmlrpcthread join" << endl;
    _xmlrpcThread->join();
    cerr << "xmlrpcthread delete" << endl;
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
        throw(nidas::core::XMLException,n_u::InvalidParameterException)
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

    // This document belongs to the caching parser
    DOMDocument* doc = parser->parse(xmlFileName);
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
    // cerr << "~DSMServer services.size=" << services.size() << endl;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	// cerr << "~DSMServer: deleting " << svc->getName() << endl;
	delete svc;
    }
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

DOMElement* DSMServer::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* DSMServer::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
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
    cerr << "interrupting services, size=" << services.size() << endl;
    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	// cerr << "doing interrupt on " << svc->getName() << endl;
	svc->interrupt();
    }
    cerr << "interrupting services done" << endl;
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
	    nservices += svc->checkSubServices();
	}
	
	cerr << "DSMServer::wait #services=" << nservices << endl;

	if (quit || restart) break;

        nanosleep(&sleepTime,0);

	if (quit || restart) break;
                                                                                
    }
    cerr << "Break out of wait loop, interrupting services" << endl;
    interruptServices();	
    cerr << "services interrupted" << endl;

    // wait a bit, then cancel whatever is still running
    sleepTime.tv_sec = 1;
    sleepTime.tv_nsec = 0;
    nanosleep(&sleepTime,0);

    cerr << "cancelling services" << endl;
    cancelServices();	
    cerr << "joining services" << endl;
    joinServices();
    cerr << "services joined" << endl;
}

/* static */
int DSMServer::parseRunstring(int argc, char** argv)
{
    debug = false;
    // extern char *optarg;	/* set by getopt() */
    extern int optind;		/* "  "     "     */
    int opt_char;		/* option character */
                                                                                
    while ((opt_char = getopt(argc, argv, "d")) != -1) {
        switch (opt_char) {
        case 'd':
            debug = true;
            break;
        case '?':
            return usage(argv[0]);
        }
    }
    if (optind == argc - 1) xmlFileName = string(argv[optind++]);
    else {
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
Usage: " << argv0 << "[-d] [config]\n\
  -d: debug. Run in foreground and send messages to stderr.\n\
      Otherwise it will run in the background and messages to syslog\n\
  config: (optional) name of DSM configuration file.\n\
          default: $NIDAS_CONFIG=\"" <<
	  	((cfg = getenv("NIDAS_CONFIG")) ? cfg : "<not set>") << endl;
    return 1;
}

