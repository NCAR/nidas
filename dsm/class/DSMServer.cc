
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
#include <atdUtil/McSocket.h>

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

	DSMServer* serverp = 0;

	try {
	    for (list<Aircraft*>::const_iterator ai=aclist.begin();
		ai != aclist.end(); ++ai) {
		Aircraft* aircraft = *ai;
		serverp = aircraft->findServer(hostname);
		if (serverp) break;
	    }

	    if (!serverp)
	    	throw atdUtil::InvalidParameterException("aircraft","server",
			string("Can't find server entry for ") + hostname);
	}
	catch (const atdUtil::Exception& e) {
	    logger->log(LOG_ERR,e.what());
	    return 1;
	}

	// make a local copy. The original is part of the Aircraft
	// object and will be deleted when we cleanup the Project.
	DSMServer server(*serverp);

	try {
	    server.scheduleServices();
	    server.wait();
	}
	catch (const atdUtil::Exception& e) {
	    logger->log(LOG_ERR,e.what());
	}

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
                                                                                
DSMServer::DSMServer()
{
}

DSMServer::~DSMServer()
{
    // delete any services that haven't been added to a listener
    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	delete svc;
    }
}
void DSMServer::fromDOMElement(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);

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
	}
    }
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();
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
		    classattr,"is not of type DSMService");
	    service->fromDOMElement((DOMElement*)child);
	    service->setServer(this);
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

void DSMServer::addThread(atdUtil::Thread* thrd)
{
    threads_lock.lock();
    threads.push_back(thrd);
    threads_lock.unlock();
}

void DSMServer::scheduleServices() throw(atdUtil::Exception)
{

    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	svc->schedule();
    }
}

void DSMServer::interrupt() throw(atdUtil::Exception)
{
    threads_lock.lock();
    list<atdUtil::Thread*> tmp = threads;
    threads_lock.unlock();

    list<atdUtil::Thread*>::iterator ti;
    for (ti = tmp.begin(); ti != tmp.end(); ++ti ) {
	atdUtil::Thread* thrd = *ti;
        thrd->interrupt();
    }
}

void DSMServer::cancel() throw(atdUtil::Exception)
{
    threads_lock.lock();
    list<atdUtil::Thread*> tmp = threads;
    threads_lock.unlock();

    list<atdUtil::Thread*>::iterator ti;
    for (ti = tmp.begin(); ti != tmp.end(); ++ti ) {
	atdUtil::Thread* thrd = *ti;
	if (thrd->isRunning()) thrd->cancel();
    }
}

void DSMServer::join() throw(atdUtil::Exception)
{

    threads_lock.lock();

    list<atdUtil::Thread*>::iterator ti;
    for (ti = threads.begin(); ti != threads.end();) {
	atdUtil::Thread* thrd = *ti;
	try {
	    thrd->join();
	}
	catch(const atdUtil::Exception& e) {
	    atdUtil::Logger::getInstance()->log(LOG_ERR,
		    "thread %s has quit, exception=%s",
	    thrd->getName().c_str(),e.what());
	}
	delete thrd;
	ti = threads.erase(ti);
    }
    threads_lock.unlock();

}

void DSMServer::wait() throw(atdUtil::Exception)
{

    // We wake up every so often and check our threads.
    // An INTR,HUP or TERM signal to this process will
    // also cause nanosleep to return. sigAction sets quit or
    // restart, and then we break, and cancel our threads.
    struct timespec sleepTime;
    sleepTime.tv_sec = 10;
    sleepTime.tv_nsec = 0;

    for (;;) {

	int nthreads = atdUtil::McSocketListener::check() +
		threads.size();
	
	cerr << "DSMServer::wait nthreads=" << nthreads << endl;

	if (quit || restart) break;

	// cerr << "DSMServer::wait nanosleep" << endl;
        nanosleep(&sleepTime,0);

	if (quit || restart) break;
                                                                                
	threads_lock.lock();

	list<atdUtil::Thread*>::iterator ti;
	for (ti = threads.begin(); ti != threads.end();) {
	    atdUtil::Thread* thrd = *ti;
	    if (!thrd->isRunning()) {
		cerr << "DSMServer::wait " << thrd->getName() << " not running" << endl;
		try {
		    thrd->join();
		}
		catch(const atdUtil::Exception& e) {
		    atdUtil::Logger::getInstance()->log(LOG_ERR,
			    "thread %s has quit, exception=%s",
		    thrd->getName().c_str(),e.what());
		}
		delete thrd;
		ti = threads.erase(ti);
	    }
	    else ++ti;
	}

	threads_lock.unlock();
    }
    cerr << "Break out of wait loop" << endl;
    interrupt();

    // wait 2 seconds, then cancel whatever is still running
    sleepTime.tv_sec = 2;
    sleepTime.tv_nsec = 0;
    nanosleep(&sleepTime,0);

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

