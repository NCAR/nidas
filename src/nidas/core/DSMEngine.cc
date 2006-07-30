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

#include <iostream>

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

/* static */
DSMEngine* DSMEngine::_instance = 0;

DSMEngine::DSMEngine():
    _syslogit(true),_wait(false),
    _interrupt(false),_runCond("_runCond"),_project(0),_dsmConfig(0),_selector(0),
    _statusThread(0),_xmlrpcThread(0),_xmlRequestSocket(0)
{
    setupSignals();
    try {
	_mcastSockAddr = n_u::Inet4SocketAddress(
	    n_u::Inet4Address::getByName(DSM_MULTICAST_ADDR),
	    DSM_SVC_REQUEST_PORT);
    }
    catch(const n_u::UnknownHostException& e) {	// shouldn't happen
        cerr << e.what();
   }

}

DSMEngine::~DSMEngine()
{
    delete _statusThread;
    delete _xmlrpcThread;
    delete _xmlRequestSocket;
    delete _selector;	// this closes any still-open sensors

    _outputMutex.lock();
    map<SampleOutput*,SampleOutput*>::const_iterator oi = _outputMap.begin();
    for ( ; oi != _outputMap.end(); ++oi) {
	SampleOutput* output = oi->first;
	SampleOutput* orig = oi->second;
	try {
	    if (output != orig) {
		output->finish();
		output->close();
		delete output;
	    }
	}
	catch(const n_u::IOException& e) {
	    n_u::Logger::getInstance()->log(LOG_INFO,
		"~DSMEngine %s: %s",output->getName().c_str(),e.what());
	}
    }
    _outputMutex.unlock();

    if (_dsmConfig) {
	const list<SampleOutput*>& outputs = _dsmConfig->getOutputs();
	list<SampleOutput*>::const_iterator oi = outputs.begin();
	for ( ; oi != outputs.end(); ++oi) {
	    SampleOutput* output = *oi;
	    try {
		output->close();	// DSMConfig will delete
	    }
	    catch(const n_u::IOException& e) {
		n_u::Logger::getInstance()->log(LOG_INFO,
		    "~DSMEngine %s: %s",output->getName().c_str(),e.what());
	    }
	}
    }
    delete _project;
}

/* static */
DSMEngine* DSMEngine::getInstance() 
{
    if (!_instance) _instance = new DSMEngine();
    return _instance;
}

/* static */
int DSMEngine::main(int argc, char** argv) throw()
{
    auto_ptr<DSMEngine> engine(getInstance());

    int res;
    if ((res = engine->parseRunstring(argc,argv)) != 0) return res;

    engine->initLogger();

    engine->run();		// doesn't throw exceptions

    // auto_ptr will call DSMEngine destructor at this point.
    return 0;
}

int DSMEngine::parseRunstring(int argc, char** argv) throw()
{
    // extern char *optarg;  /* set by getopt()  */
    extern int optind;       /*  "  "     "      */
    int opt_char;            /* option character */

    while ((opt_char = getopt(argc, argv, "dvw")) != -1) {
	switch (opt_char) {
	case 'd':
	    _syslogit = false;
	    break;
	case 'v':
	    cout << Version::getSoftwareVersion() << endl;
	    return 1;
	    break;
	case 'w':
	    _wait = true;
	    break;
	case '?':
	    usage(argv[0]);
	    return 1;
	}
    }
    if (optind == argc - 1) {
        string url = string(argv[optind++]);
	if(url.length() > 7 && !url.compare(0,7,"mcsock:")) {
	    url = url.substr(7);
	    size_t ic = url.find(':');
	    string mcastAddr = url.substr(0,ic);
	    int port = DSM_SVC_REQUEST_PORT;
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
		_mcastSockAddr = n_u::Inet4SocketAddress(
		    n_u::Inet4Address::getByName(mcastAddr),port);
	    }
	    catch(const n_u::UnknownHostException& e) {
	        cerr << e.what() << endl;
		usage(argv[0]);
		return 1;
	    }	
	}
	else _configFile = url;
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
Usage: " << argv0 << " [-dw] [ config ]\n\n\
  -d:     debug - Send error messages to stderr, otherwise to syslog\n\
  -w:     wait  - wait for the XmlRpc 'start' cammand\n\
  config: either the name of a local DSM configuration XML file to be read,\n\
      or a multicast socket address in the form \"mcsock:addr:port\".\n\
The default config is \"mcsock:" <<
	DSM_MULTICAST_ADDR << ":" << DSM_SVC_REQUEST_PORT << "\"" << endl;
}

void DSMEngine::initLogger()
{
    if (_syslogit) {
	// fork to background
	if (daemon(0,0) < 0) {
	    n_u::IOException e("DSMEngine","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
	_logger = n_u::Logger::createInstance("dsm",LOG_CONS,LOG_LOCAL5);
    }
    else
	_logger = n_u::Logger::createInstance(stderr);
}

void DSMEngine::run() throw()
{

    DOMDocument* projectDoc = 0;

    // start the xmlrpc control thread
    _xmlrpcThread = new DSMEngineIntf();
    _xmlrpcThread->start();

    // default the loop to run only once
    _quit = true;
    do {
      // purge members before re-starting the loop
      if (_selector) {
        _selector->interrupt();
        _selector->join();
        delete _selector;	// this closes any still-open sensors
        _selector = 0;
      }
      if (_project) {
        delete _project;
        _project = 0;
        _dsmConfig = 0;
      }
      // stop/join the status Thread
      if (_statusThread) {
	  _statusThread->cancel();
	  _statusThread->join();
	  delete _statusThread;
	  _statusThread = 0;
      }

      if (_wait) {
	_quit = false;
        _logger->log(LOG_DEBUG,"wait on the _runCond condition variable...");
        // wait on the _runCond condition variable
        _runCond.lock();
        while (!_run)
          _runCond.wait();
        _run = false;
        _runCond.unlock();
        if (_quit) continue;
      }

      if (projectDoc) {
        projectDoc->release();
        projectDoc = 0;
      }
      // first fetch the configuration
      try {
	if (_configFile.length() == 0)
          projectDoc = requestXMLConfig(_mcastSockAddr);
	else
          projectDoc = parseXMLConfigFile(_configFile);
      }
      catch (const XMLException& e) {
	_logger->log(LOG_ERR,e.what());
	continue;
      }
      catch (const n_u::Exception& e) {
	// DSMEngine::interrupt() does an _xmlRequestSocket->close(),
	// which will throw an IOException in requestXMLConfig 
	// if we were still waiting for the XML config.
	_logger->log(LOG_ERR,e.what());
	continue;
      }

      if (_interrupt) continue;
      // then initialize the DSMEngine
      try {
        if (projectDoc)
          initialize(projectDoc);
      }
      catch (const n_u::InvalidParameterException& e) {
	_logger->log(LOG_ERR,e.what());
	continue;
      }
      projectDoc->release();
      projectDoc = 0;

      // start your sensors
      try {
	openSensors();
	connectOutputs();
      }
      catch (const n_u::IOException& e) {
	_logger->log(LOG_ERR,e.what());
	continue;
      }
      if (_interrupt) continue;

      // start the status Thread
      _statusThread = new DSMEngineStat("DSMEngineStat");
      _statusThread->start();

      try {
	wait();
      }
      catch (const n_u::Exception& e) {
	_logger->log(LOG_ERR,e.what());
	continue;
      }
    } while (!_quit);

    if (projectDoc)
      projectDoc->release();

    _logger->log(LOG_NOTICE,"dsm shutting down");
}

void DSMEngine::mainStart()
{
  _runCond.lock();
  _run = true;
  _runCond.signal();
  _runCond.unlock();
}

void DSMEngine::mainStop()
{
  _runCond.lock();
  _run = false;
  _runCond.signal();
  _runCond.unlock();
  interrupt();
}

void DSMEngine::mainRestart()
{
  _runCond.lock();
  _run = true;
  _runCond.signal();
  _runCond.unlock();
  interrupt();
}

void DSMEngine::mainQuit()
{
  _runCond.lock();
  _run  = true;
  _runCond.signal();
  _runCond.unlock();
  interrupt();
  _quit = true;
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
    case SIGTERM:
    case SIGINT:
      DSMEngine::getInstance()->mainQuit();
      break;
    }
}

DOMDocument* DSMEngine::requestXMLConfig(
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

    delete _xmlRequestSocket;
    _xmlRequestSocket = 0;
    _xmlRequestSocket = new XMLConfigInput();
    _xmlRequestSocket->setInet4McastSocketAddress(mcastAddr);

    auto_ptr<n_u::Socket> configSock(_xmlRequestSocket->connect());
    	// throws IOException

    _xmlRequestSocket->close();
    delete _xmlRequestSocket;
    _xmlRequestSocket = 0;

    std::string sockName = configSock->getRemoteSocketAddress().toString();
    XMLFdInputSource sockSource(sockName,configSock->getFd());

    DOMDocument* doc = 0;
    try {
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

    DOMDocument* doc = parser->parse(xmlFileName);
    return doc;
}

void DSMEngine::initialize(DOMDocument* projectDoc)
	throw(n_u::InvalidParameterException)
{
    _project = Project::getInstance();

    _project->fromDOMElement(projectDoc->getDocumentElement());
    // throws n_u::InvalidParameterException;

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
    _selector->setRealTimeFIFOPriority(50);
    _selector->start();
    _dsmConfig->openSensors(_selector);
}

void DSMEngine::connectOutputs() throw(n_u::IOException)
{
    // request connection for outputs
    bool processedOutput = false;
    const list<SampleOutput*>& outputs = _dsmConfig->getOutputs();
    list<SampleOutput*>::const_iterator oi;

    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	if (!output->isRaw()) processedOutput = true;
	output->requestConnection(this);
    }
    
    const list<DSMSensor*> sensors = _selector->getAllSensors();
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
	// If we're outputting processed samples add
	// sensors as a RawSampleClient of themselves.
	if (processedOutput) sensor->addRawSampleClient(sensor);
    }
}


/* A remote system has connnected to one of our outputs.
 * We don't clone the output here.
 */
void DSMEngine::connected(SampleOutput* orig,SampleOutput* output) throw()
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"DSMEngine: connection from %s: %s",
	orig->getName().c_str(),output->getName().c_str());
    try {
	output->init();
    }
    catch (const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "DSMEngine: error in init of %s: %s",
	    	output->getName().c_str(),ioe.what());
	disconnected(output);
    }

    const list<DSMSensor*> sensors = _selector->getAllSensors();
    list<DSMSensor*>::const_iterator si;

    for (si = sensors.begin(); si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
	if (output->isRaw()) sensor->addRawSampleClient(output);
	else sensor->addSampleClient(output);
    }
    _outputMutex.lock();
    _outputMap[output] = orig;

    list<SampleOutput*>::const_iterator oi = _pendingOutputClosures.begin();
    for ( ; oi != _pendingOutputClosures.end(); oi++) {
        SampleOutput* output = *oi;
	delete output;
    }
    _pendingOutputClosures.clear();

    _outputMutex.unlock();
}

/*
 * An output wants to disconnect: probably the remote dsm_server went
 * down, or a client disconnected.
 */
void DSMEngine::disconnected(SampleOutput* output) throw()
{
    cerr << "DSMEngine::disconnected, output=" << output << endl;
    const list<DSMSensor*> sensors = _selector->getAllSensors();
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
	if (output->isRaw()) sensor->removeRawSampleClient(output);
	else sensor->removeSampleClient(output);
    }
    try {
	output->close();
    }
    catch (const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "DSMEngine: error closing %s: %s",
	    	output->getName().c_str(),ioe.what());
    }

    _outputMutex.lock();
    SampleOutput* orig = _outputMap[output];
    _outputMap.erase(output);
    if (output != orig) _pendingOutputClosures.push_back(output);
    _outputMutex.unlock();

    if (orig) orig->requestConnection(this);

    return;
}

void DSMEngine::interrupt() throw(n_u::Exception)
{
    _interrupt = true;
    n_u::Logger::getInstance()->log(LOG_INFO,
	"DSMEngine::interrupt() called");
    if (_selector) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "DSMEngine::interrupt, interrupting SensorHandler");
        _selector->interrupt();
    }
    if (_statusThread) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "DSMEngine::interrupt, cancelling status thread");
      _statusThread->cancel();
      _statusThread->join();
    }
    // If DSMEngine is waiting for an XML connection, closing the
    // _xmlRequestSocket here will cause an IOException in
    // DSMEngine::requestXMLConfig().
    if (_xmlRequestSocket) _xmlRequestSocket->close();

    n_u::Logger::getInstance()->log(LOG_INFO,
	"DSMEngine::interrupt() done");

    // do not quit when controlled by XMLRPC
    // TODO override this feature for Ctrl-C
    if (!_wait) // || signal_seen(SIGINT))
      _quit = true;
    else
      _logger->log(LOG_ERR,"DSMEngine::interrupt wait on the _runCond condition variable...");
}

void DSMEngine::wait() throw(n_u::Exception)
{
  _selector->join();
  // cerr << "DSMEngine::wait() _selector joined" << endl;
}
