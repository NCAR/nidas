/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
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
    // extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "d")) != -1) {
	switch (opt_char) {
	case 'd':
	    debug = true;
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
Usage: " << argv0 << "[-d] [config]\n\
  -d: debug (optional). Send error messages to stderr, otherwise to syslog\n\
  config: name of DSM configuration file (optional).\n\
          If config is not specified, DSM will send out\n\
          multicast requests for a configuration.\n\
" << endl;
    exit(1);
}

/* static */
DSMEngine* DSMEngine::instance = 0;

/* static */
int DSMEngine::main(int argc, char** argv) throw()
{

    DSMRunstring rstr(argc,argv);
    atdUtil::Logger* logger = 0;

    if (rstr.debug) logger = atdUtil::Logger::createInstance(stderr);
    else {
	char hostname[128];
	gethostname(hostname,sizeof(hostname));
	logger = atdUtil::Logger::createInstance(
		hostname,LOG_CONS,LOG_LOCAL5);
    }

    DSMEngine* dsm = createInstance();
    DOMDocument* projectDoc;

    // first fetch the configuration
    try {
	if (rstr.configFile.length() == 0)
		projectDoc = dsm->requestXMLConfig();
	else projectDoc = dsm->parseXMLConfigFile(rstr.configFile);
    }
    catch (const atdUtil::Exception& e) {
	logger->log(LOG_ERR,e.what());
	return 1;
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

    // then initialize the DSMEngine
    try {
	dsm->initialize(projectDoc);
    }
    catch (const atdUtil::InvalidParameterException& e) {
	logger->log(LOG_ERR,e.what());
	return 1;
    }

    projectDoc->release();

    // start your sensors
    try {
	dsm->openSensors();
	dsm->connectOutputs();
    }
    catch (const atdUtil::IOException& e) {
	logger->log(LOG_ERR,e.what());
	return 1;
    }

    try {
	dsm->wait();
    }
    catch (const atdUtil::Exception& e) {
	logger->log(LOG_ERR,e.what());
	return 1;
    }

    delete dsm;
    return 0;
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
            DSMEngine::getInstance()->interrupt();
    break;
    }
}

DSMEngine::DSMEngine():
    project(0),aircraft(0),dsmConfig(0),selector(0)
{
}

DSMEngine::~DSMEngine()
{
    cerr << "delete selector" << endl;
    delete selector;	// this closes any still-open sensors

    list<SampleOutput*>::const_iterator oi;
    for (oi = connectedOutputs.begin(); oi != connectedOutputs.end(); ++oi) {
	SampleOutput* output = *oi;
	cerr << "closing output stream" << endl;
	output->flush();
	output->close();
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
    cerr << "creating parser" << endl;
    XMLParser* parser = new XMLParser();
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

    XMLConfigInput xml;

    auto_ptr<atdUtil::Socket> configSock(xml.connect());
    	// throws IOException
    cerr << "XMLConfigInput connected" << endl;

    xml.close();

    std::string sockName = configSock->getInet4SocketAddress().toString();
    XMLFdInputSource sockSource(sockName,configSock->getFd());

    cerr << "parsing socket input" << endl;
    DOMDocument* doc = parser->parse(sockSource);
    // throws SAXException, XMLException, DOMException
    // according to xerces API doc
    // (xercesc source code doesn't have throw lists)

    cerr << "closing config socket" << endl;
    configSock->close();

    cerr << "releasing parser" << endl;
    delete parser;

    return doc;
}

DOMDocument* DSMEngine::parseXMLConfigFile(const string& xmlFileName)
	throw(atdUtil::Exception,
	DOMException,SAXException,XMLException)
{

    cerr << "creating parser" << endl;
    XMLParser* parser = new XMLParser();
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

    cerr << "releasing parser" << endl;
    delete parser;

    return doc;
}

void DSMEngine::initialize(DOMDocument* projectDoc)
	throw(atdUtil::InvalidParameterException)
{
    project = Project::getInstance();

    project->fromDOMElement(projectDoc->getDocumentElement());
    // throws atdUtil::InvalidParameterException;

    const list<Aircraft*>& aclist = project->getAircraft();
    if (aclist.size() == 0)
    	throw atdUtil::InvalidParameterException("project","aircraft",
		"no aircraft tag in XML config");

    if (aclist.size() > 1)
    	throw atdUtil::InvalidParameterException("project","aircraft",
		"multiple aircraft tags in XML config");
    aircraft = aclist.front();

    const list<DSMConfig*>& dsms = aircraft->getDSMConfigs();
    if (dsms.size() == 0)
    	throw atdUtil::InvalidParameterException("aircraft","dsm",
		"no dsm tag in XML config");
    if (dsms.size() > 1)
    	throw atdUtil::InvalidParameterException("aircraft","dsm",
		"multiple dsm tags in XML config");
    dsmConfig = dsms.front();
}

void DSMEngine::openSensors() throw(atdUtil::IOException)
{
    selector = new PortSelector;
    selector->start();
    dsmConfig->openSensors(selector);
}

void DSMEngine::connectOutputs() throw(atdUtil::IOException)
{

    // request connection for raw outputs
    const list<SampleOutput*>& routputs = dsmConfig->getRawOutputs();
    list<SampleOutput*>::const_iterator oi;

    for (oi = routputs.begin(); oi != routputs.end(); ++oi) {
	SampleOutput* output = *oi;
	output->requestConnection(this);
    }

    // request connection for other outputs
    const list<SampleOutput*>& outputs = dsmConfig->getOutputs();
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	output->requestConnection(this);
    }

    bool processSamples = outputs.size() > 0;

    const list<DSMSensor*>& sensors = dsmConfig->getSensors();
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
	// If we're outputting processed samples add
	// sensors as a RawSampleClient of themselves.
	// If it's a clock sensor, also have the clock
	// sensor process its raw samples.
	if (processSamples || sensor->isClock())
		sensor->addRawSampleClient(sensor);
    }
}

/* We don't have any SampleInputs */
void DSMEngine::connected(SampleInput* input) {}

/* A remote system has connnected to one of our outputs */
void DSMEngine::connected(SampleOutput* output)
{
    const list<DSMSensor*>& sensors = dsmConfig->getSensors();
    const list<SampleOutput*>& routputs = dsmConfig->getRawOutputs();

    list<DSMSensor*>::const_iterator si;
    list<SampleOutput*>::const_iterator oi;

    for (oi = routputs.begin(); oi != routputs.end(); ++oi) {
	SampleOutput* oput = *oi;
	if (oput == output) {
	    for (si = sensors.begin(); si != sensors.end(); ++si) {
		DSMSensor* sensor = *si;
		cerr << "adding output to sensor " << sensor->getName() << endl;
		if (sensor->isClock()) sensor->addSampleClient(output);
		else sensor->addRawSampleClient(output);
	    }
	    connectedOutputs.push_back(output);
	    return;
	}
    }

    // connect other outputs
    const list<SampleOutput*>& outputs = dsmConfig->getOutputs();
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* oput = *oi;
	if (oput == output) {
	    for (si = sensors.begin(); si != sensors.end(); ++si) {
		DSMSensor* sensor = *si;
		cerr << "adding output to sensor " << sensor->getName() << endl;
		sensor->addSampleClient(output);
	    }
	    connectedOutputs.push_back(output);
	    return;
	}
    }
}

void DSMEngine::interrupt() throw(atdUtil::Exception)
{
    if (selector) {
	atdUtil::Logger::getInstance()->log(LOG_INFO,
	    "DSMEngine::interrupt received, interrupting PortSelector");
        selector->interrupt();
    }
    else {
	atdUtil::Logger::getInstance()->log(LOG_INFO,
	    "DSMEngine::interrupt received, exiting");
        exit(1);
    }
}

void DSMEngine::wait() throw(atdUtil::Exception)
{
    selector->join();
    cerr << "selector joined" << endl;
}

