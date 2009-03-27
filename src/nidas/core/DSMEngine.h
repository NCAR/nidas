/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_CORE_DSMENGINE_H
#define NIDAS_CORE_DSMENGINE_H

#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/SensorHandler.h>
#include <nidas/core/StatusThread.h>
#include <nidas/core/XMLConfigInput.h>
#include <nidas/core/DSMEngineIntf.h>
#include <nidas/core/XMLException.h>
#include <nidas/core/DerivedDataReader.h>

#include <nidas/util/Socket.h>
#include <nidas/util/Logger.h>

#include <xercesc/dom/DOMDocument.hpp>

namespace nidas { namespace core {

/**
 * A singleton class that drives an ADS3 data collection box.
 */
class DSMEngine : public SampleConnectionRequester {
public:

    /**
     * Entry point to run a DSMEngine process from a command line.
     * main creates an instance of DSMEngine, passes it the
     * command line arguments and calls the run method.
     */
    static int main(int argc, char** argv) throw();

    /**
     * Get a pointer to the singleton instance of DSMEngine.
     * This will create the instance if it doesn't exist.
     */
    static DSMEngine* getInstance();

    /**
     * Nuke it.
     */
    virtual ~DSMEngine();

    /**
     * Initialize the Logger.
     */
    void initLogger();

    /** main loop */
    int run() throw();

    /**
     * Parse the runstring parameters.
     * If the runstring parameters are not OK, then DSMEngine::usage()
     * is called to print a message to stderr, and this method
     * then returns a error value of 1.
     * @return 0: OK, 1: failure.
     */
    int parseRunstring(int argc, char** argv) throw();

    /**
     * Print runstring usage to stderr.
     */
    void usage(const char* argv0);

    /** Then main wait while the sensors are running */
    void wait() throw(nidas::util::Exception);

    /** Starts the main loop (for the XMLRPC call). */
    void start();

    /** Stops the main loop (for the XMLRPC call). */
    void stop();

    /** Restarts the main loop (for the XMLRPC call). */
    void restart();

    /** Quits the main loop (for the XMLRPC call). */
    void quit();

    SampleClock* getSampleClock() { return _clock; }

    const DSMConfig* getDSMConfig() const { return _dsmConfig; }

    const SensorHandler* getSensorHandler() const { return _selector; }

    /**
     * Request the XML configuration via a McSocket request to
     * a given multicast socket address.
     */
    xercesc::DOMDocument* requestXMLConfig(const nidas::util::Inet4SocketAddress&)
	throw(nidas::util::Exception);

    /**
     * This function is used as a utility and does not need an instance
     * of DSMEngine.
     */
    static xercesc::DOMDocument* parseXMLConfigFile(const std::string& xmlFileName)
	throw(nidas::core::XMLException);

    /**
     * Is system running RTLinux?  Checks if rtl module is loaded.
     */
    static bool isRTLinux();

    static std::string getUserName()
    { 
        return _username;
    }

    static uid_t getUserID()
    {
        return _userid;
    }

    static uid_t getGroupID()
    {
        return _groupid;
    }

private:

    /**
     * The protected constructor, called from getInstance.
     */
    DSMEngine();

    /**
     * Initialize the DSMEngine based on the parameters in the
     * DOMDocument.  This method initializes the Project object,
     * _project from the DOM, and sets the value of _dsmConfig.
     */
    void initialize(xercesc::DOMDocument* projectDoc)
            throw(nidas::util::InvalidParameterException);

    void openSensors() throw(nidas::util::IOException);

    void connectOutputs() throw(nidas::util::IOException);

    void connectProcessors() throw(nidas::util::IOException);

    void disconnectProcessors() throw();

    void closeOutputs() throw();

    void interrupt();

    void deleteDataThreads() throw();

    /**
     * Implementation of ConnectionRequester connected methods.
     * This is how DSMEngine is notified of remote connections.
     */
    void connected(SampleOutput*,SampleOutput*) throw();

    void disconnected(SampleOutput*) throw();

    static void setupSignals();

    /** Signal handler */
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static DSMEngine* _instance;

    bool _externalControl;

    enum run_states { CONFIG, INIT, RUNNING, STOPPED, ERROR } _runState;

    enum next_states { STOP, START, QUIT, RESTART } _nextState;

    /**
     * Whether to log messages on syslog (true) or stderr (false).
     * Set to false from -d runstring option, otherwise true.
     */
    bool _syslogit;

    /** Name of XML configuration file. If empty, multicast for config. */
    std::string _configFile;

    /**
     * Address to use when fishing for the XML configuration.
     */
    nidas::util::Inet4SocketAddress _configSockAddr;

    /**
     * Condition variable to wait on for external command.
     */
    nidas::util::Cond _runCond;

    Project*         _project;

    DSMConfig*       _dsmConfig;

    SensorHandler*    _selector;

    /**
     * A thread that generates streaming XML time and status.
     */
    DSMEngineStat*    _statusThread;

    /** This thread provides XML-based Remote Procedure calls */
    DSMEngineIntf*   _xmlrpcThread;

    SampleClock*    _clock;

    /**
     * Mapping between connected outputs and the original
     * outputs.
     */
    std::map<SampleOutput*,SampleOutput*> _outputMap;

    std::list<SampleOutput*> _pendingOutputClosures;

    nidas::util::Mutex            _outputMutex;

    XMLConfigInput*           _xmlRequestSocket;

    nidas::util::Mutex         _xmlRequestMutex;

    nidas::util::Logger*          _logger;

    /**
     * Cached result for isRTLinux. -1 means it has not been determined yet.
     */
    static int rtlinux;

    std::list<SampleInputWrapper*> _inputs;

    static std::string _username;

    static uid_t _userid;

    static gid_t _groupid;

};

}}	// namespace nidas namespace core

#endif

