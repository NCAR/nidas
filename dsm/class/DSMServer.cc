
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <DSMServer.h>

#include <Aircraft.h>

#include <DSMTime.h>
#include <XMLParser.h>
#include <DOMObjectFactory.h>

#include <atdUtil/InvalidParameterException.h>
#include <atdUtil/Logger.h>

#include <unistd.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

/* static */
std::string DSMServer::xmlFileName;

/* static */
bool DSMServer::quit = false;

/* static */
bool DSMServer::restart = false;

/* static */
int DSMServer::main(int argc, char** argv) throw()
{
                                                                                
    DSMServerRunstring rstr(argc,argv);
    atdUtil::Logger* logger = 0;

    char hostname[MAXHOSTNAMELEN];
    gethostname(hostname,sizeof(hostname));
                                                                                
    if (rstr.debug) logger = atdUtil::Logger::createInstance(stderr);
    else {
        logger = atdUtil::Logger::createInstance(
                hostname,LOG_CONS,LOG_LOCAL5);
    }

    setXMLFileName(rstr.configFile);

    setupSignals();

    while (!quit) {

	Project* project;
	try {
	    project = parseXMLConfigFile();
	}
	catch (const SAXException& e) {
	    logger->log(LOG_ERR,
		    XMLStringConverter(e.getMessage()));
	    return 1;
	}
	catch (const DOMException& e) {
	    logger->log(LOG_ERR,
		    XMLStringConverter(e.getMessage()));
	    return 1;
	}
	catch (const XMLException& e) {
	    logger->log(LOG_ERR,
		    XMLStringConverter(e.getMessage()));
	    return 1;
	}
	catch(const atdUtil::InvalidParameterException& e) {
	    logger->log(LOG_ERR,e.what());
	    return 1;
	}
	catch (const atdUtil::Exception& e) {
	    logger->log(LOG_ERR,e.what());
	    return 1;
	}

	const list<Aircraft*>& aclist = project->getAircraft();

	DSMServer* server = 0;

	for (list<Aircraft*>::const_iterator ai=aclist.begin();
	    ai != aclist.end(); ++ai) {
	    Aircraft* aircraft = *ai;
	    server = aircraft->findServer(hostname);
	    if (server) break;
	}


	try {
	    if (!server)
	    	throw atdUtil::InvalidParameterException("aircraft","server",
			string("Can't find server entry for ") + hostname);
	    server->startServices();
	}
	catch (const atdUtil::Exception& e) {
	    logger->log(LOG_ERR,e.what());
	    return 1;
	}

	server->wait();

	delete project;
    }
    return 0;
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
    atdUtil::Logger::getInstance()->log(LOG_INFO,
        "received signal %s(%d), si_signo=%d, si_errno=%d, si_code=%d",
        strsignal(sig),sig,
        (siginfo ? siginfo->si_signo : -1),
        (siginfo ? siginfo->si_errno : -1),
        (siginfo ? siginfo->si_code : -1));
                                                                                
    switch(sig) {
    case SIGHUP:
	DSMServer::setRestart(true);
	break;
    case SIGTERM:
    case SIGINT:
    case SIGUSR1:
	DSMServer::setQuit(true);
	break;
    }
}

/* static */
Project* DSMServer::parseXMLConfigFile()
        throw(atdUtil::Exception,
        DOMException,SAXException,XMLException,
	atdUtil::InvalidParameterException)
{
    XMLCachingParser* parser = XMLCachingParser::getInstance();
    // throws Exception, DOMException
                                                                                
    // If parsing a local file, turn on validation
    parser->setDOMValidation(true);
    parser->setDOMValidateIfSchema(true);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(true);
    parser->setXercesSchemaFullChecking(true);
    parser->setDOMDatatypeNormalization(false);
    parser->setXercesUserAdoptsDOMDocument(true);
                                                                                
    // This document belongs to the caching parser
    DOMDocument* doc = parser->parse(getXMLFileName());
                                                                                
    Project* project = Project::getInstance();
                                                                                
    project->fromDOMElement(doc->getDocumentElement());
    // throws atdUtil::InvalidParameterException;

    return project;
}
                                                                                
void DSMServer::fromDOMElement(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{
#ifdef XML_DEBUG
    cerr << "DSMServer::fromDOMElement start -------------" << endl;
#endif
    XDOMElement xnode(node);

#ifdef XML_DEBUG
    cerr << "DSMServer::fromDOMElement element name=" <<
    	xnode.getNodeName() << endl;
#endif
	
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    const std::string& aname = attr.getName();
	    const std::string& aval = attr.getValue();
	    if (!aname.compare("name")) setName(aval);
#ifdef XML_DEBUG
	    cerr << "attrname=" << aname << " val=" << aval << endl;
#endif
	}
    }
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();
#ifdef XML_DEBUG
	cerr << "element name=" << elname << endl;
#endif
	if (!elname.compare("service")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0) 
		throw atdUtil::InvalidParameterException(
		    "DSMServer::fromDOMElement",
			elname,
			"does not have a class attribute");
	    DSMService* service;
	    try {
		service = dynamic_cast<DSMService*>
			(DOMObjectFactory::createObject(classattr));
	    }
	    catch (const atdUtil::Exception& e) {
		throw atdUtil::InvalidParameterException("service",
		    classattr,e.what());
	    }
	    if (!service)
		throw atdUtil::InvalidParameterException("service",
		    classattr,"is no of type DSMService");
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

void DSMServer::startServices() throw(atdUtil::Exception)
{

    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	atdUtil::ServiceListener* listener = svc->getServiceListener();
	serviceListeners.insert(listener);
    }
}

struct restartInfo {
    dsm_sys_time_t lastRestart;
    float rate;	// restarts per millisecond
    int nrestarts;
};

void DSMServer::cancel() throw(atdUtil::Exception)
{
    set<atdUtil::ServiceListener*>::iterator si;
    for (si = serviceListeners.begin(); si != serviceListeners.end(); ++si) {
	atdUtil::ServiceListener* serv = *si;
	serv->cancel();
    }
}

void DSMServer::join() throw(atdUtil::Exception)
{
    set<atdUtil::ServiceListener*>::iterator si;
    for (si = serviceListeners.begin(); si != serviceListeners.end(); ++si) {
	atdUtil::ServiceListener* serv = *si;
	serv->join();
    }
}
void DSMServer::wait() throw(atdUtil::Exception)
{
    map<atdUtil::ServiceListener*,struct restartInfo> restarts;

    dsm_sys_time_t tnow = getCurrentTimeInMillis();
    set<atdUtil::ServiceListener*>::iterator si;
    for (si = serviceListeners.begin(); si != serviceListeners.end(); ++si) {
	atdUtil::ServiceListener* serv = *si;
	struct restartInfo rsi;
	rsi.lastRestart = tnow;
	rsi.nrestarts = 1;
        restarts[serv] = rsi;
    }

    float maxRestartRate = .001;	// restarts per millisecond

    // ServiceListeners are supposed to send a SIGUSR1 signal when
    // they have finished running. That will cause a
    // EINTR return from nanosleep.  If they don't
    // we'll wake up every so often and check our threads.
    // An INTR,HUP or TERM signal to this process will
    // also cause nanosleep to return. sigAction sets quit or
    // restart, and then we break, and cancel our threads.
    struct timespec sleepTime;
    sleepTime.tv_sec = 10;
    sleepTime.tv_nsec = 0;

    while (serviceListeners.size() > 0) {
	cerr << "DSMServer::wait serviceListeners.size=" <<
		serviceListeners.size() << endl;
	if (quit || restart) break;
        // pause();
	cerr << "DSMServer::wait nanosleep" << endl;
        nanosleep(&sleepTime,0);

	if (quit || restart) break;
                                                                                
        set<atdUtil::ServiceListener*>::iterator si;
        for (si = serviceListeners.begin(); si != serviceListeners.end(); ++si) {
            atdUtil::ServiceListener* serv = *si;
	    cerr << "DSMServer::wait checking ServiceListener serv=" <<
	    	(void*)serv << endl;
            if (!serv->isRunning()) {
		cerr << "DSMServer::wait ServiceListener not running" << endl;
		try {
		    serv->join();
		}
		catch(const atdUtil::Exception& e) {
		    atdUtil::Logger::getInstance()->log(LOG_ERR,"service %s has quit, exception=%s",
			serv->getName().c_str(),e.what());
		}

#ifdef IMPLEMENT_RESTART
		if (serv->needsRestart()) {
		    tnow = getCurrentTimeInMillis();
		    struct restartInfo rsi = restarts[serv];
		    int dt = tnow - rsi.lastRestart;

		    // do a approximated mean of last 10 restarts
		    int nr = rsi.nrestarts++;
		    if (nr > 10) nr = 10;
		    rsi.rate = ((nr - 1) * rsi.rate  + 1./dt) / nr;

		    if (nr == 10 && rsi.rate > maxRestartRate) {
			atdUtil::Logger::getInstance()->log(LOG_ERR,"service %s has been started %d times at a rate of %f starts/sec. Somethings wrong. Deleting service",
			    serv->getName().c_str(),rsi.nrestarts,rsi.rate*1000.);
			serviceListeners.erase(si);	// iterator is invalid
			break;
		    }
		    else {
			atdUtil::Logger::getInstance()->log(LOG_WARNING,"restarting service %s",
			    serv->getName().c_str());
			serv->start();
		    }
		}
		else {
		    serviceListeners.erase(si);	// iterator is invalid
		    break;
		}
#else
		serviceListeners.erase(si);	// iterator is invalid
		break;
#endif
            }
        }
    }
    cancel();
    join();
}

DSMServerRunstring::DSMServerRunstring(int argc, char** argv) {
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
            usage(argv[0]);
        }
    }
    if (optind != argc - 1) usage(argv[0]);
    configFile = string(argv[optind++]);
}

/* static */
void DSMServerRunstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-d] config\n\
  -d: debug (optional). Send error messages to stderr, otherwise to syslog\n\
  config: name of DSM configuration file (required).\n\
" << endl;
    exit(1);
}

