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
#include <nidas/core/DerivedDataReader.h>
#include <nidas/core/XMLConfigInput.h>
#include <nidas/core/DSMEngineIntf.h>
#include <nidas/core/SensorHandler.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/core/SampleOutputRequestThread.h>

namespace nidas { namespace core {

/**
 * Application for running the NIDAS data acquistion process.
 */
class DSMEngine : public SampleConnectionRequester {
public:

    DSMEngine();

    /**
     * Nuke it.
     */
    ~DSMEngine();

    /**
     * Entry point to run a DSMEngine process from a command line.
     * main creates an instance of DSMEngine, passes it the
     * command line arguments and calls the run method.
     */
    static int main(int argc, char** argv) throw();

    /**
     * Get a pointer to the singleton instance of DSMEngine created
     * by main().
     */
    static DSMEngine* getInstance() { return _instance; }

    static void setupSignals();

    static void unsetupSignals();

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
    bool isRTLinux();

    std::string getUserName()
    { 
        return _username;
    }

    uid_t getUserID()
    {
        return _userid;
    }

    uid_t getGroupID()
    {
        return _groupid;
    }

private:

    /** Signal handler */
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static DSMEngine* _instance;

    /**
     * Initialize the DSMEngine based on the parameters in the
     * DOMDocument.  This method initializes the Project object,
     * _project from the DOM, and sets the value of _dsmConfig.
     */
    void initialize(xercesc::DOMDocument* projectDoc)
            throw(nidas::util::InvalidParameterException);

    void startXmlRpcThread() throw(nidas::util::Exception);

    void killXmlRpcThread() throw();

    void openSensors() throw(nidas::util::IOException);

    void connectOutputs() throw(nidas::util::IOException);

    void connectProcessors() throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    void disconnectProcessors() throw();

    void closeOutputs() throw();

    void interrupt();

    void deleteDataThreads() throw();

    void joinDataThreads() throw();

    /**
     * Implementation of SampleConnectionRequester connect methods.
     * This is how DSMEngine is notified of remote connections.
     */
    void connect(SampleOutput*) throw();

    void disconnect(SampleOutput*) throw();

    /**
     * DSMEngine does not receive SampleInputs, so this will die with an assert.
     */
    void connect(SampleInput*) throw() { assert(false); }

    /**
     * DSMEngine does not receive SampleInputs, so these will die with an assert.
     */
    void disconnect(SampleInput*) throw() { assert(false); }

    bool _externalControl;

    enum run_states { RUNNING, ERROR, STOPPED } _runState;

    enum next_states { STOP, RUN, QUIT, RESTART } _nextState;

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
     * Condition variable to wait on for external command or signal.
     */
    nidas::util::Cond _runCond;

    Project*         _project;

    DSMConfig*       _dsmConfig;

    SensorHandler*  _selector;

    SamplePipeline* _pipeline;

    /**
     * A thread that generates streaming XML time and status.
     */
    DSMEngineStat*    _statusThread;

    /** This thread provides XML-based Remote Procedure calls */
    DSMEngineIntf*   _xmlrpcThread;

    SampleClock*    _clock;

    /**
     * Connected SampleOutputs
     */
    std::set<SampleOutput*> _outputSet;

    nidas::util::Mutex            _outputMutex;

    XMLConfigInput*           _xmlRequestSocket;

    nidas::util::Mutex         _xmlRequestMutex;

    /**
     * Cached result for isRTLinux. -1 means it has not been determined yet.
     */
    int _rtlinux;


#ifdef NEEDED
    std::list<DSMSensorWrapper*> _inputs;
#endif

    std::string _username;

    uid_t _userid;

    gid_t _groupid;

};

}}	// namespace nidas namespace core

#endif

