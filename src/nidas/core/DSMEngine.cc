// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/Config.h>

#include "DSMEngine.h"
#include "Project.h"
#include "Site.h"
#include "DSMConfig.h"
#include "StatusThread.h"
#include "DSMEngineIntf.h"
#include "DerivedDataReader.h"
#include "SensorHandler.h"
#include "SamplePipeline.h"
#include "requestXMLConfig.h"

#include "XMLStringConverter.h"
#include "XMLParser.h"
#include "Version.h"

#include "XMLConfigInput.h"
#include "XMLFdInputSource.h"

#include "SampleIOProcessor.h"
#include "NidsIterators.h"
#include "SampleOutputRequestThread.h"
#include <nidas/util/Process.h>
#include <nidas/util/FileSet.h>

#include <iostream>
#include <fstream>
#include <limits>
#include <sys/resource.h>
#include <sys/mman.h>

#include <unistd.h>  // for getopt(), optind, optarg

#ifdef HAVE_SYS_CAPABILITY_H 
#include <sys/capability.h>
#include <sys/prctl.h>
#endif 

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;
namespace n_c = nidas::core;

namespace {
    int defaultLogLevel = n_u::LOGGER_NOTICE;
};

/* static */
DSMEngine* DSMEngine::_instance = 0;

DSMEngine::DSMEngine():
    _externalControl(false),_disableAutoconfig(false),_runState(DSM_RUNNING),
    _command(DSM_RUN),_syslogit(true),_configFile(),_configSockAddr(),
    _project(0), _dsmConfig(0),_selector(0),_pipeline(0),
    _statusThread(0),_xmlrpcThread(0),
    _outputSet(),_outputMutex(),
    _logLevel(defaultLogLevel),_signalMask(),_myThreadId(::pthread_self()),
    _app("dsm")
{
    try {
	_configSockAddr = n_u::Inet4SocketAddress(
	    n_u::Inet4Address::getByName(NIDAS_MULTICAST_ADDR),
	    NIDAS_SVC_REQUEST_PORT_UDP);
    }
    catch(const n_u::UnknownHostException& e) {	// shouldn't happen
        cerr << e.what();
    }
    setupSignals();
}

DSMEngine::~DSMEngine()
{
    delete _statusThread;
    delete _xmlrpcThread;
    SampleOutputRequestThread::destroyInstance();
    delete _project;
    _project = 0;
    SamplePools::deleteInstance();
}

namespace {
    void getPageFaults(long& minor,long& major, long& nswap) 
    {
        struct rusage r;
        getrusage(RUSAGE_SELF,&r);
        minor = r.ru_minflt;
        major = r.ru_majflt;
        nswap = r.ru_nswap;
    }
    void logPageFaultDiffs(long minor,long major, long nswap)
    {
        long minflts,majflts,nswap2;
        getPageFaults(minflts,majflts,nswap2);

        n_u::Logger::getInstance()->log(LOG_INFO,"page faults: minor=%d, major=%d, swaps=%d",
            minflts-minor,majflts-major,nswap2-nswap);
    }
}

/* static */
int DSMEngine::main(int argc, char** argv) throw()
{
    DSMEngine engine;

    int res;
    if ((res = engine.parseRunstring(argc,argv)) != 0) return res;

    // If the user has not selected -d (debug), initLogger will fork
    // to the background, using daemon(). After the fork, threads other than this
    // main thread will not be running, unless they use pthread_atfork().
    // So, in general, don't start any threads before this.
    engine.initLogger();

    if ((res = engine.initProcess()) != 0) return res;

    long minflts,majflts,nswap;
    getPageFaults(minflts,majflts,nswap);

    // Set the singleton instance
    _instance = &engine;

    try {
        engine.startXmlRpcThread();

        res = engine.run();		// doesn't throw exceptions

        engine.killXmlRpcThread();
    }
    catch (const n_u::Exception &e) {
        PLOG(("%s",e.what()));
    }

    // Should figure out how to delete this automagically.
    DSMSensor::deleteLooper();

    XMLImplementation::terminate();

    SamplePoolInterface* charPool = SamplePool<SampleT<char> >::getInstance();
    ILOG(("dsm: sample pools: #s%d,#m%d,#l%d,#o%d\n",
                    charPool->getNSmallSamplesIn(),
                    charPool->getNMediumSamplesIn(),
                    charPool->getNLargeSamplesIn(),
                    charPool->getNSamplesOut()));

    logPageFaultDiffs(minflts,majflts,nswap);

    if (engine.getCommand() == DSM_SHUTDOWN) n_u::Process::spawn("halt");
    else if (engine.getCommand() == DSM_REBOOT) n_u::Process::spawn("reboot");

    // All users of singleton instance should have been shut down.
    _instance = 0;

    // Hack: wait for detached threads to delete themselves, so that
    // valgrind --leak-check=full doesn't complain.
    {
        struct timespec slp = {0, NSECS_PER_SEC / 50};
        nanosleep(&slp,0);
    }

    return res;
}

int DSMEngine::parseRunstring(int argc, char** argv) throw()
{
    NidasAppArg ExternalControl
        ("-r,--remote", "",
         "Start XML RPC thread to enable to remote commands.");
    NidasAppArg DisableAutoConfig
        ("-n,--no-autoconfig", "", 
         "Disable autoconfig by removing all "
         "<autoconfig> tags from the DOM \n"
         "before invoking fromDOMElement()"
         "and setting xml class names back \n"
         "to DSMSerialSensor or other original value");

    _app.enableArguments(_app.loggingArgs() | _app.Version | _app.Help |
                         _app.Username | _app.Hostname | _app.DebugDaemon | 
                         ExternalControl | DisableAutoConfig);

    ArgVector args = _app.parseArgs(argc, argv);
    if (_app.helpRequested())
    {
        usage();
        return 1;
    }
    _externalControl = ExternalControl.asBool();
    _disableAutoconfig = DisableAutoConfig.asBool();

    
    if (args.size() == 1)
    {
        string url = string(args[0]);
        string type = "file";
        string::size_type ic = url.find(':');
        if (ic != string::npos) type = url.substr(0,ic);
        if (type == "sock" || type == "inet" || type == "mcsock") {
	    url = url.substr(ic+1);
	    ic = url.find(':');
	    string addr = url.substr(0,ic);
            if (addr.length() == 0 && type == "mcsock") addr = NIDAS_MULTICAST_ADDR;
	    int port = NIDAS_SVC_REQUEST_PORT_UDP;
	    if (ic != string::npos) {
		istringstream ist(url.substr(ic+1));
		ist >> port;
		if (ist.fail()) {
		    cerr << "Invalid port number: " << url.substr(ic+1) << endl;
		    usage();
		    return 1;
		}
	    }
	    try {
		_configSockAddr = n_u::Inet4SocketAddress(
		    n_u::Inet4Address::getByName(addr),port);
                // cerr << "sock addr=" << _configSockAddr.toString() << endl;
	    }
	    catch(const n_u::UnknownHostException& e) {
	        cerr << e.what() << endl;
		usage();
		return 1;
	    }	
	}
        else if (type == "file") _configFile = url;
        else {
            cerr << "unknown url: " << url << endl;
            usage();
            return 1;
        }
    }
    else if (args.size() > 1)
    {
	usage();
	return 1;
    }
    return 0;
}

void DSMEngine::usage() 
{
    cerr <<
        "Usage: " << _app.getName() << " [options] [config]\n"
        "\n"
        "config:\n"
        "  The name of a local DSM configuration XML file\n"
        "  to be read, or a socket address in the form \"sock:addr:port\".\n"
        "  The default config is "
        "\"sock:" <<
        NIDAS_MULTICAST_ADDR << ":" <<
        NIDAS_SVC_REQUEST_PORT_UDP << "\"" << endl;
    cerr << "\nOptions:\n";
    cerr << _app.usage();
}

void DSMEngine::initLogger()
{
    _app.setupDaemon();
}

int DSMEngine::initProcess()
{
    _app.setupProcess();

    if (_app.checkPidFile() != 0)
    {
        return 1;
    }
    _app.lockMemory();

    return 0;
}

int DSMEngine::run() throw()
{
    xercesc::DOMDocument* projectDoc = 0;

    SampleOutputRequestThread::getInstance()->start();

    int res = 0;

    for (; !quitCommand(_command); ) {

        // cleanup before re-starting the loop
        joinDataThreads();

        disconnectProcessors();

        closeOutputs();

        if (projectDoc) {
            projectDoc->release();
            projectDoc = 0;
        }

        delete _project;
        _project = 0;
        _dsmConfig = 0;

        // One of the data threads is the SensorHandler. Deleting the SensorHandler
        // deletes all the sensors. They should be deleted after the
        // project is deleted since various objects in the project may
        // hold references to the sensors.
        deleteDataThreads();

        while (_command == DSM_STOP) waitForSignal(0);

        if (_runState == DSM_ERROR && !quitCommand(_command)) {
            waitForSignal(15);
            if (quitCommand(_command)) {
                res = 1;    // quit after an error. Return non-zero result
                break;
            }
            _command = DSM_RUN;
        }

        
        if (_command == DSM_RESTART) _command = DSM_RUN;
        if (_command != DSM_RUN) continue;

        // first fetch the configuration
        try {
            if (_configFile.length() == 0) {
                projectDoc = n_c::requestXMLConfig(false,_configSockAddr, &_signalMask);
            }
            else {
                // expand environment variables in name
                string expName = n_u::Process::expandEnvVars(_configFile);
                projectDoc = parseXMLConfigFile(expName);
            }
        }
        catch (const XMLException& e) {
            PLOG(("%s",e.what()));
            _runState = DSM_ERROR;
            continue;
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
            _runState = DSM_ERROR;
            continue;
        }

        if (_command != DSM_RUN) continue;

        // then initialize the DSMEngine
        try {
            initialize(projectDoc);
        }
        catch (const n_u::InvalidParameterException& e) {
            PLOG(("%s",e.what()));
            _runState = DSM_ERROR;
            continue;
        }
        projectDoc->release();
        projectDoc = 0;
        XMLImplementation::terminate();

        res = 0;

        if (_dsmConfig->getDerivedDataSocketAddr().getPort() != 0) {
              DerivedDataReader::createInstance(_dsmConfig->getDerivedDataSocketAddr());
	}
        // start your sensors
        try {
            openSensors();
            connectOutputs();
            connectProcessors();
        }
        catch (const n_u::IOException& e) {
            PLOG(("%s",e.what()));
            _runState = DSM_ERROR;
            continue;
        }
        catch (const n_u::InvalidParameterException& e) {
            PLOG(("%s",e.what()));
            _runState = DSM_ERROR;
            continue;
        }
        if (_command != DSM_RUN) continue;

        // start the status Thread
        if (_dsmConfig->getStatusSocketAddr().getPort() != 0) {
            _statusThread = new DSMEngineStat("DSMEngineStat",_dsmConfig->getStatusSocketAddr());
            _statusThread->start();
	}
        _runState = DSM_RUNNING;

        while (_command == DSM_RUN) {
            waitForSignal(0);
        }

        if (quitCommand(_command)) break;
        interrupt();
    }   // Run loop

    interrupt();

    joinDataThreads();

    disconnectProcessors();

    closeOutputs();

    if (projectDoc) {
        projectDoc->release();
        projectDoc = 0;
    }

    delete _project;
    _project = 0;
    _dsmConfig = 0;

    deleteDataThreads();

    return res;
}

void DSMEngine::interrupt()
{
    if (_statusThread) _statusThread->interrupt();
    if (DerivedDataReader::getInstance()) DerivedDataReader::getInstance()->interrupt();
    if (_selector) _selector->interrupt();
    if (_pipeline) {
        _pipeline->flush();
        _pipeline->interrupt();
    }
}

void DSMEngine::joinDataThreads() throw()
{
    // stop/join the status thread before closing sensors.
    // The status thread also loops over sensors.
    if (_statusThread) {
        try {
            DLOG(("DSMEngine joining statusThread"));
            _statusThread->join();
            DLOG(("DSMEngine statusThread joined"));
        }
        catch (const n_u::Exception& e) {
            WLOG(("%s",e.what()));
        }
    }

    if (_selector) {
        try {
            DLOG(("DSMEngine joining selector"));
            _selector->join();
            DLOG(("DSMEngine selector joined"));
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
        }
    }

    if (_pipeline) {
        try {
            DLOG(("DSMEngine joining pipeline"));
            _pipeline->join();
            DLOG(("DSMEngine pipeline joined"));
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
        }
    }

    if (DerivedDataReader::getInstance()) {
        // If clients of DerivedDataReader are doing something that
        // blocks, it might still be running.
        try {
            for (int i = 0; i < 10; i++) {
                if (!DerivedDataReader::getInstance()->isRunning()) break;
                usleep(USECS_PER_SEC/10);
            }
            if (DerivedDataReader::getInstance()->isRunning()) {
                PLOG(("DerivedDataReader is still running, cancelling"));
                DerivedDataReader::getInstance()->cancel();
            }
        }
        catch (const n_u::Exception& e) {
            WLOG(("%s",e.what()));
        }

        try {
            DLOG(("DSMEngine joining DerivedDataReader"));
            DerivedDataReader::getInstance()->join();
            DLOG(("DSMEngine DerivedDataReader joined"));
        }
        catch (const n_u::Exception& e) {
            PLOG(("%s",e.what()));
        }
    }
}

void DSMEngine::deleteDataThreads() throw()
{
    delete _statusThread;
    _statusThread = 0;

    delete _selector;	// this closes any still-open sensors
    _selector = 0;

    if (DerivedDataReader::getInstance()) {
        DerivedDataReader::deleteInstance();
    }

    delete _pipeline;
    _pipeline = 0;
}

void DSMEngine::start()
{
    _command = DSM_RUN;
    pthread_kill(_myThreadId,SIGUSR1);
}

/*
 * Stop data acquisition threads and wait for next command.
 */
void DSMEngine::stop()
{
    _command = DSM_STOP;
    pthread_kill(_myThreadId,SIGUSR1);
}

void DSMEngine::restart()
{
    pthread_kill(_myThreadId,SIGHUP);
}

void DSMEngine::quit()
{
    pthread_kill(_myThreadId,SIGTERM);
}

void DSMEngine::shutdown()
{
    _command = DSM_SHUTDOWN;
    pthread_kill(_myThreadId,SIGUSR1);
}

void DSMEngine::reboot()
{
    _command = DSM_REBOOT;
    pthread_kill(_myThreadId,SIGUSR1);
}

void DSMEngine::setupSignals()
{
    // unblock these with sigwaitinfo/sigtimedwait in waitForSignal
    sigemptyset(&_signalMask);
    sigaddset(&_signalMask,SIGUSR1);
    sigaddset(&_signalMask,SIGHUP);
    sigaddset(&_signalMask,SIGTERM);
    sigaddset(&_signalMask,SIGINT);

    // block them otherwise
    pthread_sigmask(SIG_BLOCK,&_signalMask,0);
}

void DSMEngine::waitForSignal(int timeoutSecs)
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

    ILOG(("DSMEngine received signal ") << strsignal(sig) << '(' << sig << ')');
    switch(sig) {
    case SIGHUP:
	_command = DSM_RESTART;
	break;
    case SIGTERM:
    case SIGINT:
	_command = DSM_QUIT;
        break;
    case SIGUSR1:
        // an XMLRPC method could set _command and send SIGUSR1
	break;
    default:
        WLOG(("DSMEngine received unknown signal:") << strsignal(sig));
        break;
    }
}

void DSMEngine::startXmlRpcThread() throw(n_u::Exception)
{
    // start the xmlrpc control thread
    if (_externalControl) {
        _xmlrpcThread = new DSMEngineIntf();
        _xmlrpcThread->start();
    }
}

void DSMEngine::killXmlRpcThread() throw()
{
    if (!_xmlrpcThread) return;
    _xmlrpcThread->interrupt();
    try {
        DLOG(("DSMEngine joining xmlrpcThread"));
       _xmlrpcThread->join();
        DLOG(("DSMEngine xmlrpcThread joined"));
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s",e.what()));
    }

    delete _xmlrpcThread;
   _xmlrpcThread = 0;
}

void DSMEngine::registerSensorWithXmlRpc(const std::string& devname,DSMSensor* sensor)
{
    if (_xmlrpcThread) return _xmlrpcThread->registerSensor(devname,sensor);
}

void DSMEngine::removeAutoConfigObjects(xercesc::DOMNode* node, bool bumpRecursion) 
{
    static int recursionLevel = 0;
    xercesc::DOMNode* pChild;
    xercesc::DOMElement* pElementNode = dynamic_cast<xercesc::DOMElement*>(node);

    if (bumpRecursion) {
        // should get here for any invocation within this method
        ++recursionLevel;
    }
    else {
        // should only happen on first invocation from outside this method
        if (pElementNode) {
            XDOMElement xnode(pElementNode);
            if (xnode.getNodeName() != "project") {
                throw n_u::InvalidParameterException(
                    "DSMEngine::removeAutoConfigObjects(): ","starting xml node name not \"project\"",
                        xnode.getNodeName());
            }
            else {
                ILOG(("DSMEngine::removeAutoConfigObjects(): Getting off on the right foot. First tag: ") 
                        << xnode.getNodeName());
            }
        }
        else {
            throw n_u::InvalidParameterException(
                "DSMEngine::removeAutoConfigObjects(): ","starting xml node not element tag",
                    XMLStringConverter(node->getNodeName()));
        }
    }

    VLOG(("DSMEngine::removeAutoConfigObjects(): recursion depth is: ") << recursionLevel);

    std::string classValue;
    int numElementChildren = 0;
    pChild = node->getFirstChild();
    if (!pChild) {
        VLOG(("DSMEngine::removeAutoConfigObjects(): Root node has no children. All done. Get outta here."));
        --recursionLevel;
        return;
    }

    for (pChild = node->getFirstChild(); pChild != 0;
         pChild = pChild->getNextSibling(), ++numElementChildren) {
        VLOG(("DSMEngine::removeAutoConfigObjects(): checking element child #") << numElementChildren+1);

        // nothing interesting to do if not a <serialSensor> element
        if (pChild->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) {
            // except check its child elements
            VLOG(("DSMEngine::removeAutoConfigObjects(): Node is not an element tag, so recurse down..."));
            removeAutoConfigObjects(pChild, true);
            continue;
        }

        XDOMElement xChild(dynamic_cast<xercesc::DOMElement*>(pChild));
        if (xChild.getNodeName() != std::string("serialSensor")) {
            VLOG(("DSMEngine::removeAutoConfigObjects(): Element node is not named serialSensor, so recurse down..."));
            removeAutoConfigObjects(pChild, true);
            continue;
        }
        else {
            VLOG(("DSMEngine::removeAutoConfigObjects(): found element named: ") << xChild.getNodeName());
        }

        // landed on a <serialSensor> tag, so if the sensor class is one of the 
        // values called out below, warp it back to DSMSerialSensor.
        VLOG(("DSMEngine::removeAutoConfigObjects(): Looking for class values that need to be reset..."));
        std::string classValue = xChild.getAttributeValue("class");
        if (classValue.length()) {
            if (classValue == "isff.PTB210" || classValue == "isff.PTB220" ) {
                ILOG(("DSMEngine::removeAutoConfigObjects(): Resetting ") << classValue << " to DSMSerialSensor");
                // Change the class to instantiate to non-autoconfig
                xChild.setAttributeValue("class", "DSMSerialSensor");
            }
            else if (classValue == "isff.GILL2D") {
                xChild.setAttributeValue("class", "isff.PropVane");
                ILOG(("DSMEngine::removeAutoConfigObjects(): resetting class value to isff.PropVane for: ") << classValue);
            }
            else {
                VLOG(("DSMEngine::removeAutoConfigObjects(): Skipping class value: ") << classValue);
            }
        }
        else {
            VLOG(("DSMEngine::removeAutoConfigObjects(): No attributes named \"class\" to check in this serial sensor..."));
        }

        VLOG(("DSMEngine::removeAutoConfigObjects(): Also check element for an <autoconfig> tag to remove"));
        xercesc::DOMNode* pSensorChild = pChild->getFirstChild();
        if (!pSensorChild) {
            VLOG(("DSMEngine::removeAutoConfigObjects(): serialSensor element has no sub-children."));
            continue;
        }

        int numSubElementChild = 0;
        for (; pSensorChild != 0; 
               pSensorChild = pSensorChild->getNextSibling(), ++numSubElementChild) {
            VLOG(("DSMEngine::removeAutoConfigObjects(): Checking subElement child #") << numSubElementChild+1);
            if (pSensorChild->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) {
                removeAutoConfigObjects(pSensorChild, true);
                continue;
            }
            
            XDOMElement xChild(dynamic_cast<xercesc::DOMElement*>(pSensorChild));
            if (xChild.getNodeName() != "autoconfig") {
                removeAutoConfigObjects(pSensorChild, true);
                continue;
            }

            pChild->removeChild(pSensorChild);
            ILOG(("DSMEngine::removeAutoConfigObjects(): removed <autoconfig> tag from: ") << classValue);
            break; // should only be one <autoconfig> tag
        }
    }
    VLOG(("DSMEngine::removeAutoConfigObjects(): Done checking at recursion level: ") << recursionLevel);
    --recursionLevel;
}

void DSMEngine::initialize(xercesc::DOMDocument* projectDoc)
	throw(n_u::InvalidParameterException)
{
    _project = new Project();

    if (_disableAutoconfig) {
        ILOG(("DSMEngine::initialize(): _disableAutoconfig is true. Pull all the <autoconfig> tags out of DOM"));
        xercesc::DOMNode* node = projectDoc->getDocumentElement();
        removeAutoConfigObjects(node);
    }

     _project->fromDOMElement(projectDoc->getDocumentElement());
    // throws n_u::InvalidParameterException;
    if (_configFile.length() > 0)
	_project->setConfigName(_configFile);

    std::string hostname = _app.getHostName();
    _dsmConfig = _project->findDSMFromHostname(hostname);
    if (!_dsmConfig)
    {
    	throw n_u::InvalidParameterException("dsm","no match for hostname",
                                             hostname);
    }
}

void DSMEngine::openSensors() throw(n_u::IOException)
{
    _selector = new SensorHandler(_dsmConfig->getRemoteSerialSocketPort());

    n_u::Logger::getInstance()->log(LOG_INFO,"DSMEngine: setting RT priority");
    _selector->setRealTimeFIFOPriority(50);

    _pipeline = new SamplePipeline();
    _pipeline->setRealTime(true);
    _pipeline->setRawSorterLength(_dsmConfig->getRawSorterLength());
    _pipeline->setProcSorterLength(_dsmConfig->getProcSorterLength());
    _pipeline->setRawHeapMax(_dsmConfig->getRawHeapMax());
    _pipeline->setProcHeapMax(_dsmConfig->getProcHeapMax());
    _pipeline->setRawLateSampleCacheSize(_dsmConfig->getRawLateSampleCacheSize());
    _pipeline->setProcLateSampleCacheSize(_dsmConfig->getProcLateSampleCacheSize());

    _pipeline->setKeepStats(false);

    /* Initialize pipeline with all expected SampleTags. */
    const list<DSMSensor*> sensors = _dsmConfig->getSensors();
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
        _pipeline->connect(sensor);
    }
    _selector->start();
    _dsmConfig->openSensors(_selector);
}

void DSMEngine::connectOutputs() throw(n_u::IOException)
{
    // request connection for outputs
    const list<SampleOutput*>& outputs = _dsmConfig->getOutputs();
    list<SampleOutput*>::const_iterator oi;

    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	DLOG(("DSMEngine requesting connection from SampleOutput '%s'.",
		     output->getName().c_str()));
        SampleSource* src;
        if (output->isRaw()) src = _pipeline->getRawSampleSource();
        else src = _pipeline->getProcessedSampleSource();
        output->addSourceSampleTags(src->getSampleTags());
        SampleOutputRequestThread::getInstance()->addConnectRequest(output,this,0);
    }
}

/* implementation of SampleConnectionRequester::connect(SampleOutput*) */
void DSMEngine::connect(SampleOutput* output) throw()
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"DSMEngine: connection from %s", output->getName().c_str());

    _outputMutex.lock();
    _outputSet.insert(output);
    _outputMutex.unlock();

    if (output->isRaw()) _pipeline->getRawSampleSource()->addSampleClient(output);
    else  _pipeline->getProcessedSampleSource()->addSampleClient(output);
}

/*
 * An output wants to disconnect: probably the remote dsm_server went
 * down, or a client disconnected.
 */
void DSMEngine::disconnect(SampleOutput* output) throw()
{
    if (output->isRaw()) _pipeline->getRawSampleSource()->removeSampleClient(output);
    else  _pipeline->getProcessedSampleSource()->removeSampleClient(output);

    _outputMutex.lock();
    _outputSet.erase(output);
    _outputMutex.unlock();

    output->flush();
    try {
	output->close();
    }
    catch (const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "DSMEngine: error closing %s: %s",
	    	output->getName().c_str(),ioe.what());
    }

    SampleOutput* orig = output->getOriginal();

    if (output != orig)
       SampleOutputRequestThread::getInstance()->addDeleteRequest(output);

    int delay = orig->getReconnectDelaySecs();
    if (delay < 0) return;
    SampleOutputRequestThread::getInstance()->addConnectRequest(orig,this,delay);
}

void DSMEngine::closeOutputs() throw()
{

    _outputMutex.lock();

    set<SampleOutput*>::const_iterator oi = _outputSet.begin();
    for ( ; oi != _outputSet.end(); ++oi) {
	SampleOutput* output = *oi;
        if (output->isRaw()) _pipeline->getRawSampleSource()->removeSampleClient(output);
        else  _pipeline->getProcessedSampleSource()->removeSampleClient(output);
        output->flush();
	try {
            output->close();
	}
	catch(const n_u::IOException& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,
		"%s: %s",output->getName().c_str(),e.what());
	}
        SampleOutput* orig = output->getOriginal();
        if (output != orig) delete output;
    }
    _outputSet.clear();
    _outputMutex.unlock();

    if (_dsmConfig) {
	const list<SampleOutput*>& outputs = _dsmConfig->getOutputs();
	list<SampleOutput*>::const_iterator oi = outputs.begin();
	for ( ; oi != outputs.end(); ++oi) {
	    SampleOutput* output = *oi;
            output->flush();
	    try {
		output->close();	// DSMConfig will delete
	    }
	    catch(const n_u::IOException& e) {
		n_u::Logger::getInstance()->log(LOG_ERR,
		    "%s: %s",output->getName().c_str(),e.what());
	    }
	}
    }
    SampleOutputRequestThread::getInstance()->clear();
}

void DSMEngine::connectProcessors() throw(n_u::IOException,n_u::InvalidParameterException)
{
    ProcessorIterator pi = _dsmConfig->getProcessorIterator();

    // If there are one or more processors defined, call sensor init methods.
    if (pi.hasNext()) {
        SensorIterator si = _dsmConfig->getSensorIterator();
        for (; si.hasNext(); ) {
            DSMSensor* sensor = si.next();
            sensor->init();
        }
    }

    // establish connections for processors
    for ( ; pi.hasNext(); ) {
        SampleIOProcessor* proc = pi.next();
        proc->connect(_pipeline);
    }
}

void DSMEngine::disconnectProcessors() throw()
{
    if (_dsmConfig) {
        ProcessorIterator pi = _dsmConfig->getProcessorIterator();
        for ( ; pi.hasNext(); ) {
            SampleIOProcessor* proc = pi.next();
            proc->disconnect(_pipeline);
            proc->flush();
        }
    }
}

