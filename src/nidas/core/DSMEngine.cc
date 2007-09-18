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
#include <nidas/util/Process.h>

#include <iostream>
#include <fstream>
#include <limits>

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

/* static */
DSMEngine* DSMEngine::_instance = 0;

/* static */
int DSMEngine::rtlinux = -1;	// unknown

DSMEngine::DSMEngine():
    _externalControl(false),_runState(STOPPED),_nextState(START),
    _syslogit(true),
    _project(0),
    _dsmConfig(0),_selector(0),
    _statusThread(0),_xmlrpcThread(0),
    _clock(SampleClock::getInstance()),
    _xmlRequestSocket(0)
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
		output->finish();
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

    // Open and check the pid file after the above daemon() call.
    try {
        pid_t pid = n_u::Process::checkPidFile("/tmp/dsm.pid");
        if (pid > 0) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                "dsm process, pid=%d is already running",pid);
            return 1;
        }
    }
    catch(const n_u::IOException& e) {
        n_u::Logger::getInstance()->log(LOG_ERR,"dsm: %s",e.what());
        return 1;
    }

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
            _externalControl = true;
	    _nextState = STOP;
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
	    string::size_type ic = url.find(':');
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
Usage: " << argv0 << " [-d ] [-v] [-w] [ config ]\n\n\
  -d:     debug - Send error messages to stderr, otherwise to syslog\n\
  -v:     display software version number and exit\n\
  -w:     wait  - wait for the XmlRpc 'start' cammand\n\
  config: either the name of a local DSM configuration XML file to be read,\n\
      or a multicast socket address in the form \"mcsock:addr:port\".\n\
The default config is \"mcsock:" <<
	DSM_MULTICAST_ADDR << ":" << DSM_SVC_REQUEST_PORT << "\"" << endl;
}

void DSMEngine::initLogger()
{
    n_u::LogConfig lc;
    if (_syslogit) {
	// fork to background
	if (daemon(0,0) < 0) {
	    n_u::IOException e("DSMEngine","daemon",errno);
	    cerr << "Warning: " << e.toString() << endl;
	}
	_logger = n_u::Logger::createInstance("dsm",LOG_CONS,LOG_LOCAL5);
        // Configure default logging to log anything NOTICE and above.
        lc.level = n_u::LOGGER_INFO;
    }
    else
    {
	_logger = n_u::Logger::createInstance(&std::cerr);
        lc.level = n_u::LOGGER_DEBUG;
        cerr << "not syslog" << endl;
    }

    _logger->setScheme(n_u::LogScheme().addConfig (lc));
}

void DSMEngine::run() throw()
{
    DOMDocument* projectDoc = 0;

    // start the xmlrpc control thread
    if (_externalControl) {
        _xmlrpcThread = new DSMEngineIntf();
        _xmlrpcThread->start();
    }

    for (; _nextState != QUIT; ) {

        if (_runState == ERROR && _nextState != STOP) sleep(15);

        // cleanup before re-starting the loop
        deleteDataThreads();

        if (projectDoc) {
            projectDoc->release();
            projectDoc = 0;
        }
        if (_project) {
            delete _project;
            _project = 0;
            _dsmConfig = 0;
        }

        if (_nextState == STOP) {
            _runState = STOPPED;
            // wait on the _runCond condition variable
            _runCond.lock();
            while (_nextState == STOP) _runCond.wait();
            _runCond.unlock();
        }
        if (_nextState == RESTART) _nextState = START;
        if (_nextState != START) continue;
        _runState = CONFIG;

        // first fetch the configuration
        try {
            if (_configFile.length() == 0)
                projectDoc = requestXMLConfig(_mcastSockAddr);
            else {
                // expand environment variables in name
                string expName = Project::expandEnvVars(_configFile);
                projectDoc = parseXMLConfigFile(expName);
            }
        }
        catch (const XMLException& e) {
            _logger->log(LOG_ERR,e.what());
            _runState = ERROR;
            continue;
        }
        catch (const n_u::Exception& e) {
            // DSMEngine::interrupt() does an _xmlRequestSocket->close(),
            // which will throw an IOException in requestXMLConfig 
            // if we were still waiting for the XML config.
            _logger->log(LOG_ERR,e.what());
            _runState = ERROR;
            continue;
        }
        _runState = INIT;

        if (_nextState != START) continue;

        // then initialize the DSMEngine
        try {
            initialize(projectDoc);
        }
        catch (const n_u::InvalidParameterException& e) {
            _logger->log(LOG_ERR,e.what());
            _runState = ERROR;
            continue;
        }
        projectDoc->release();
        projectDoc = 0;

        if (_dsmConfig->getDerivedDataSocketAddr().getPort() != 0) {
	    try {
              DerivedDataReader::createInstance(_dsmConfig->getDerivedDataSocketAddr());
	    }
	    catch(n_u::IOException&e) {
		_logger->log(LOG_ERR,e.what());
	    }
	}
        // start your sensors
        try {
            openSensors();
            connectOutputs();
        }
        catch (const n_u::IOException& e) {
            _logger->log(LOG_ERR,e.what());
            _runState = ERROR;
            continue;
        }
        if (_nextState != START) continue;

        // start the status Thread
        _statusThread = new DSMEngineStat("DSMEngineStat");
        _statusThread->start();
        _runState = RUNNING;

        try {
            _selector->join();
        }
        catch (const n_u::Exception& e) {
            _logger->log(LOG_ERR,e.what());
        }
    }   // Run loop

    interrupt();
    deleteDataThreads();

    if (projectDoc) {
        projectDoc->release();
        projectDoc = 0;
    }
    if (_project) {
        delete _project;
        _project = 0;
        _dsmConfig = 0;
    }

    if (_xmlrpcThread) {
        try {
            if (_xmlrpcThread->isRunning()) {
                n_u::Logger::getInstance()->log(LOG_INFO,
                    "DSMEngine::interrupt, cancelling xmlrpcThread");
                // if this is running under valgrind, then cancel doesn't
                // work, but a kill(SIGUSR1) does.  Otherwise a cancel works.
                // _xmlrpcThread->cancel();
                _xmlrpcThread->kill(SIGUSR1);
            }
            n_u::Logger::getInstance()->log(LOG_INFO,
                "DSMEngine::interrupt, joining xmlrpcThread");
           _xmlrpcThread->join();
        }
        catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_WARNING,
            "xmlRpcThread: %s",e.what());
        }
    }

    _logger->log(LOG_NOTICE,"dsm shutting down");
}

void DSMEngine::interrupt()
{
    // If DSMEngine is waiting for an XML connection, closing the
    // _xmlRequestSocket here will cause an IOException in
    // DSMEngine::requestXMLConfig().
    if (_xmlRequestSocket) _xmlRequestSocket->close();

    if (_statusThread) _statusThread->interrupt();
    if (DerivedDataReader::getInstance()) DerivedDataReader::getInstance()->interrupt();
    if (_selector) _selector->interrupt();
}

void DSMEngine::deleteDataThreads()
{
    // stop/join the status Thread. The status thread also loops
    // over sensors.
    if (_statusThread) {
        try {
            if (_statusThread->isRunning()) _statusThread->kill(SIGUSR1);
            _statusThread->join();
        }
        catch (const n_u::Exception& e) {
            _logger->log(LOG_ERR,e.what());
        }
        delete _statusThread;
        _statusThread = 0;
    }

    if (_selector) {
        try {
            _selector->join();
        }
        catch (const n_u::Exception& e) {
            _logger->log(LOG_ERR,e.what());
        }
        delete _selector;	// this closes any still-open sensors
        _selector = 0;
    }

    if (DerivedDataReader::getInstance()) {
        try {
            if (DerivedDataReader::getInstance()->isRunning())
                DerivedDataReader::getInstance()->kill(SIGUSR1);
            DerivedDataReader::getInstance()->join();
            DerivedDataReader::deleteInstance();
        }
        catch (const n_u::Exception& e) {
            _logger->log(LOG_ERR,e.what());
        }
    }
}

void DSMEngine::start()
{
    _runCond.lock();
    _nextState = START;
    _runCond.signal();
    _runCond.unlock();
}

/*
 * Stop data acquisition threads and wait for next command.
 */
void DSMEngine::stop()
{
    _runCond.lock();
    _nextState = STOP;
    _runCond.signal();
    _runCond.unlock();
    interrupt();
}

void DSMEngine::restart()
{
    _runCond.lock();
    _nextState = RESTART;
    _runCond.signal();
    _runCond.unlock();
    interrupt();
}

void DSMEngine::quit()
{
    _runCond.lock();
    _nextState = QUIT;
    _runCond.signal();
    _runCond.unlock();
    interrupt();
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
      DSMEngine::getInstance()->restart();
      break;
    case SIGTERM:
    case SIGINT:
      DSMEngine::getInstance()->quit();
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
    if (_configFile.length() > 0)
	_project->setConfigName(_configFile);

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
    /*
     * Can't use real-time FIFO priority in user space via
     * pthread_setschedparam under RTLinux. How ironic.
     * It causes ENOSPC on the RTLinux fifos.
     */
     if (!isRTLinux()) {
        n_u::Logger::getInstance()->log(LOG_INFO,
            "DSMEngine: !RTLinux, so setting RT priority");
         _selector->setRealTimeFIFOPriority(50);
     }
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
	_logger->log(LOG_DEBUG,
		     "DSMEngine requesting connection from SampleOutput '%s'.",
		     output->getName().c_str());
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


/* static */
bool DSMEngine::isRTLinux()
{
    if (rtlinux >= 0) return rtlinux > 0;

    ifstream modfile("/proc/modules");

    // check to see if rtl module is loaded.
    while (!modfile.eof() && !modfile.fail()) {
        string module;
        modfile >> module;
        if (module == "rtl") {
	    rtlinux = 1;
	    return true;
	}
        modfile.ignore(std::numeric_limits<int>::max(),'\n');
    }
    rtlinux = 0;
    return false;
}

