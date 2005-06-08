/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <DSMEngine.h>

#include <atdUtil/Logger.h>

#include <XMLStringConverter.h>
#include <XMLParser.h>

#include <XMLConfigInput.h>
#include <XMLFdInputSource.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

DSMRunstring::DSMRunstring(int argc, char** argv) {
    debug = false;
    wait  = false;
    // extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "dw")) != -1) {
	switch (opt_char) {
	case 'd':
	    debug = true;
	    break;
	case 'w':
	    wait = true;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }
    if (optind == argc - 1) configFile = string(argv[optind++]);

    if (optind != argc) usage(argv[0]);
}

/* static */
void DSMRunstring::usage(const char* argv0) 
{
    cerr << "\
Usage: " << argv0 << " [-dw] [config]\n\n\
  -d:     debug - Send error messages to stderr, otherwise to syslog\n\
  -w:     wait  - wait for the XmlRpc 'start' cammand\n\n\
  config: name of DSM configuration file (optional).\n\
          If config is not specified, DSM will send out\n\
          multicast requests for a configuration.\n\
" << endl;
    exit(1);
}

/* static */
bool DSMEngine::quit = false;

/* static */
bool DSMEngine::run = false;

/* static */
atdUtil::Cond DSMEngine::runCond("runCond");

/* static */
DSMEngine* DSMEngine::instance = 0;

/* static */
int DSMEngine::main(int argc, char** argv) throw()
{
    cerr << "compiled on " << __DATE__ << " at " << __TIME__ << endl;

    DSMRunstring rstr(argc,argv);
    atdUtil::Logger* logger = 0;

    if (rstr.debug) logger = atdUtil::Logger::createInstance(stderr);
    else {
	char hostname[128];
	gethostname(hostname,sizeof(hostname));
	logger = atdUtil::Logger::createInstance(
		hostname,LOG_CONS,LOG_LOCAL5);
    }

    auto_ptr<DSMEngine> dsm(createInstance());
    DOMDocument* projectDoc;

    // start the xmlrpc control thread
    dsm->xmlrpcThread = new XmlRpcThread("DSMEngineXmlRpc");
    dsm->xmlrpcThread->start();

    while (!quit) {

      if (rstr.wait) {
        cerr << "wait on the runCond condition variable...\n";
        // wait on the runCond condition variable
        runCond.lock();
        while (!run)
          runCond.wait();
        run = false;
        runCond.unlock();
        if (quit) break;

      }
      cerr << "DSMEngine: first fetch the configuration" << endl;
      // first fetch the configuration
      try {
	if (rstr.configFile.length() == 0)
          projectDoc = dsm->requestXMLConfig();
	else
          projectDoc = dsm->parseXMLConfigFile(rstr.configFile);
      }
      catch (const atdUtil::Exception& e) {
	// DSMEngine::interrupt() does an xmlRequestSocket->close(),
	// which will throw an IOException in requestXMLConfig 
	// if we were still waiting for the XML config.
	logger->log(LOG_ERR,e.what());
	continue;
      }
      catch (const SAXException& e) {
	logger->log(LOG_ERR,
                    XMLStringConverter(e.getMessage()));
	continue;
      }
      catch (const DOMException& e) {
	logger->log(LOG_ERR,
                    XMLStringConverter(e.getMessage()));
	continue;
      }
      catch (const XMLException& e) {
	logger->log(LOG_ERR,
                    XMLStringConverter(e.getMessage()));
	continue;
      }
      cerr << "DSMEngine: then initialize the DSMEngine" << endl;
      // then initialize the DSMEngine
      try {
	dsm->initialize(projectDoc);
      }
      catch (const atdUtil::InvalidParameterException& e) {
	logger->log(LOG_ERR,e.what());
	continue;
      }

      projectDoc->release();

      cerr << "DSMEngine: start your sensors" << endl;
      // start your sensors
      try {
	dsm->openSensors();
	dsm->connectOutputs();
      }
      catch (const atdUtil::IOException& e) {
	logger->log(LOG_ERR,e.what());
	continue;
      }
      cerr << "DSMEngine: dsm->wait()" << endl;
      try {
	dsm->wait();
      }
      catch (const atdUtil::Exception& e) {
	logger->log(LOG_ERR,e.what());
	continue;
      }
    }
    cerr << "DSMEngine::main() exiting..." << endl;
    return 0;
}

void DSMEngine::mainStart()
{
  runCond.lock();
  run = true;
  runCond.signal();
  runCond.unlock();
}

void DSMEngine::mainStop()
{
  runCond.lock();
  run = false;
  runCond.signal();
  runCond.unlock();
  interrupt();
}

void DSMEngine::mainRestart()
{
  runCond.lock();
  run = true;
  runCond.signal();
  runCond.unlock();
  DSMEngine::getInstance()->interrupt();
}

void DSMEngine::mainQuit()
{
  runCond.lock();
  quit = true;
  run  = true;
  runCond.signal();
  runCond.unlock();
  DSMEngine::getInstance()->interrupt();
}

/* static */
void DSMEngine::setupSignals()
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
    act.sa_sigaction = DSMEngine::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

void DSMEngine::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    atdUtil::Logger::getInstance()->log(LOG_INFO,
    	"received signal %s(%d), si_signo=%d, si_errno=%d, si_code=%d",
	strsignal(sig),sig,
	(siginfo ? siginfo->si_signo : -1),
	(siginfo ? siginfo->si_errno : -1),
	(siginfo ? siginfo->si_code : -1));
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
      DSMEngine* pDSMEngine = DSMEngine::getInstance();
      pDSMEngine->runCond.lock();
      pDSMEngine->quit = true;
      pDSMEngine->run  = true;
      pDSMEngine->runCond.signal();
      pDSMEngine->runCond.unlock();
      pDSMEngine->interrupt();
      break;
    }
}

DSMEngine::DSMEngine():
    project(0),site(0),dsmConfig(0),selector(0),statusThread(0),
    xmlrpcThread(0),xmlRequestSocket(0)
{
}

DSMEngine::~DSMEngine()
{
    cerr << "deleting threads...\n";
    if (xmlrpcThread) {
      cerr << "deleting threads...xmlrpcThread->cancel()\n";
      xmlrpcThread->cancel();
      cerr << "deleting threads...xmlrpcThread->join()\n";
      xmlrpcThread->join();
    }
    cerr << "deleting xmlrpcThread\n";
    delete xmlrpcThread;
    cerr << "deleting xmlRequestSocket\n";
    delete xmlRequestSocket;
    cerr << "deleting statusThread\n";
    delete statusThread;
    cerr << "deleting selector\n";
    delete selector;	// this closes any still-open sensors
    cerr << "deleting...done\n";

    outputMutex.lock();
    list<SampleOutput*>::const_iterator oi;
    for (oi = connectedOutputs.begin(); oi != connectedOutputs.end(); ++oi) {
	SampleOutput* output = *oi;
	try {
	    output->flush();
	    output->close();
	}
	catch(const atdUtil::IOException& e) {
	    atdUtil::Logger::getInstance()->log(LOG_INFO,
		"~DSMEngine %s: %s",output->getName().c_str(),e.what());
	}
    }
    outputMutex.unlock();

    if (dsmConfig) {
	const list<SampleOutput*>& outputs = dsmConfig->getOutputs();
	for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	    SampleOutput* output = *oi;
	    try {
		output->close();
	    }
	    catch(const atdUtil::IOException& e) {
		atdUtil::Logger::getInstance()->log(LOG_INFO,
		    "~DSMEngine %s: %s",output->getName().c_str(),e.what());
	    }
	}
    }

    cerr << "delete project" << endl;
    delete project;
}

DSMEngine* DSMEngine::createInstance()
{
  if (!instance) {
    instance = new DSMEngine();
    setupSignals();
  }
  return instance;
}

DSMEngine* DSMEngine::getInstance() 
{
    return instance;
}

DOMDocument* DSMEngine::requestXMLConfig()
	throw(atdUtil::Exception,
	    DOMException,SAXException,XMLException)
{
    // cerr << "creating parser" << endl;
    auto_ptr<XMLParser> parser(new XMLParser());
    // throws Exception, DOMException

    // If parsing xml received from a server over a socket,
    // turn off validation - assume the server has validated the XML.
    parser->setDOMValidation(false);
    parser->setDOMValidateIfSchema(false);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(false);
    parser->setXercesSchemaFullChecking(false);
    parser->setDOMDatatypeNormalization(false);
    parser->setXercesUserAdoptsDOMDocument(true);

    delete xmlRequestSocket;
    xmlRequestSocket = new XMLConfigInput();

    auto_ptr<atdUtil::Socket> configSock(xmlRequestSocket->connect());
    	// throws IOException

    xmlRequestSocket->close();
    delete xmlRequestSocket;
    xmlRequestSocket = 0;

    std::string sockName = configSock->getInet4SocketAddress().toString();
    XMLFdInputSource sockSource(sockName,configSock->getFd());

    cerr << "parsing socket input" << endl;
    DOMDocument* doc = 0;
    try {
	// throws SAXException, XMLException, DOMException
	// according to xerces API doc
	// (xercesc source code doesn't have throw lists)
	doc = parser->parse(sockSource);
    }
    catch(...) {
	configSock->close();
	throw;
    }

    configSock->close();
    return doc;
}

/* static */
DOMDocument* DSMEngine::parseXMLConfigFile(const string& xmlFileName)
	throw(atdUtil::Exception,
	DOMException,SAXException,XMLException)
{

    // cerr << "creating parser" << endl;
    auto_ptr<XMLParser> parser(new XMLParser());
    // throws Exception, DOMException

    // If parsing a local file, turn on validation
    parser->setDOMValidation(true);
    parser->setDOMValidateIfSchema(true);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(true);
    parser->setXercesSchemaFullChecking(true);
    parser->setDOMDatatypeNormalization(false);
    parser->setXercesUserAdoptsDOMDocument(true);

    DOMDocument* doc = parser->parse(xmlFileName);

    return doc;
}

void DSMEngine::initialize(DOMDocument* projectDoc)
	throw(atdUtil::InvalidParameterException)
{
    project = Project::getInstance();

    project->fromDOMElement(projectDoc->getDocumentElement());
    // throws atdUtil::InvalidParameterException;

    const list<Site*>& sitelist = project->getSites();
    if (sitelist.size() == 0)
    	throw atdUtil::InvalidParameterException("project","site",
		"no site tag in XML config");

    if (sitelist.size() > 1)
    	throw atdUtil::InvalidParameterException("project","site",
		"multiple site tags in XML config");
    site = sitelist.front();

    const list<DSMConfig*>& dsms = site->getDSMConfigs();
    if (dsms.size() == 0)
    	throw atdUtil::InvalidParameterException("site","dsm",
		"no dsm tag in XML config");
    if (dsms.size() > 1)
    	throw atdUtil::InvalidParameterException("site","dsm",
		"multiple dsm tags in XML config");
    dsmConfig = dsms.front();
}

void DSMEngine::openSensors() throw(atdUtil::IOException)
{
    selector = new PortSelector;
    selector->start();
    dsmConfig->openSensors(selector);

    // start the status Thread
    statusThread = new StatusThread("DSMEngineStatus",10);
    statusThread->start();
}

void DSMEngine::connectOutputs() throw(atdUtil::IOException)
{

    // request connection for outputs
    bool processedOutput = false;
    const list<SampleOutput*>& outputs = dsmConfig->getOutputs();
    list<SampleOutput*>::const_iterator oi;

    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	if (!output->isRaw()) processedOutput = true;
	output->requestConnection(this);
    }
    
    const list<DSMSensor*>& sensors = dsmConfig->getSensors();
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
	// If we're outputting processed samples add
	// sensors as a RawSampleClient of themselves.
	// If it's a clock sensor, also have the clock
	// sensor process its raw samples.
	if (processedOutput) sensor->addRawSampleClient(sensor);
    }
}


/* A remote system has connnected to one of our outputs.
 * We don't clone the output here.
 */
void DSMEngine::connected(SampleOutput* output) throw()
{
    cerr << "SampleOutput " << hex << output << dec << " " <<
    	output->getName() << " connected" << " fd=" << output->getFd() << endl;

    try {
	output->init();
    }
    catch (const atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "DSMEngine: error in init of %s: %s",
	    	output->getName().c_str(),ioe.what());
	disconnected(output);
    }

    const list<DSMSensor*>& sensors = dsmConfig->getSensors();
    list<DSMSensor*>::const_iterator si;

    for (si = sensors.begin(); si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
	cerr << "adding output to sensor " << sensor->getName() << endl;
	if (output->isRaw()) sensor->addRawSampleClient(output);
	else sensor->addSampleClient(output);
    }
    outputMutex.lock();
    connectedOutputs.push_back(output);
    outputMutex.unlock();
}

/* An output wants to disconnect (probably the remote server went down) */
void DSMEngine::disconnected(SampleOutput* output) throw()
{
    cerr << "SampleOutput " << output->getName() << " disconnected" << endl;
    const list<DSMSensor*>& sensors = dsmConfig->getSensors();
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
	cerr << "removing output from sensor " << sensor->getName() << endl;
	if (output->isRaw()) sensor->removeRawSampleClient(output);
	else sensor->removeSampleClient(output);
    }
    try {
	output->close();
    }
    catch (const atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "DSMEngine: error closing %s: %s",
	    	output->getName().c_str(),ioe.what());
    }

    outputMutex.lock();
    list<SampleOutput*>::iterator oi;
    for (oi = connectedOutputs.begin(); oi != connectedOutputs.end();) {
	if (*oi == output) oi = connectedOutputs.erase(oi);
	else ++oi;
    }
    outputMutex.unlock();

    // try to reconnect (should probably restart and request XML again)
    output->requestConnection(this);
    return;
}

void DSMEngine::interrupt() throw(atdUtil::Exception)
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"DSMEngine::interrupt() called");
    if (selector) {
	atdUtil::Logger::getInstance()->log(LOG_INFO,
	    "DSMEngine::interrupt, interrupting PortSelector");
        selector->interrupt();
    }
    if (statusThread) statusThread->cancel();

    // If DSMEngine is waiting for an XML connection, closing the
    // xmlRequestSocket here will cause an IOException in
    // DSMEngine::requestXMLConfig().
    if (xmlRequestSocket) xmlRequestSocket->close();

    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"DSMEngine::interrupt() done");
}

void DSMEngine::wait() throw(atdUtil::Exception)
{
    selector->join();
    cerr << "selector joined" << endl;
    statusThread->join();
    cerr << "statusThread joined" << endl;
}
