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

#include <nidas/core/ConnectionRequester.h>
#include <nidas/core/XMLException.h>
#include <nidas/util/Inet4SocketAddress.h>
#include <nidas/util/InvalidParameterException.h>

#include <set>

#include <signal.h>

namespace nidas { namespace core {

class Project;
class DSMConfig;
class DSMSensor;
class SensorHandler;
class SamplePipeline;
class DSMEngineIntf;
class XMLConfigInput;
class SampleClock;
class DSMEngineStat;

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

    /**
     * Initialize the Logger.
     */
    void initLogger();

    /**
     * Initialize various process parameters, uid, etc.
     */
    int initProcess(const char* argv0);

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

    /** Starts the main loop (for the XMLRPC call). */
    void start();

    /** Stops the main loop (for the XMLRPC call). */
    void stop();

    /** Restarts the main loop (for the XMLRPC call). */
    void restart();

    /** Quit the main loop. */
    void quit();

    /** Quits the main loop, and spawns a "halt" shell command. */
    void shutdown();

    /** Quits the main loop, and spawns a "reboot" shell command. */
    void reboot();

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

    /**
     * Sensors register with the DSMEngineIntf XmlRpcThread if they have a
     * executeXmlRpc() method which can be invoked with a "SensorAction"
     * XmlRpc request.
     * @param devname: string matching the "device" XmlRpc parameter,
     *  typically the device name.
     */
    void registerSensorWithXmlRpc(const std::string& devname,DSMSensor*);

    enum command { DSM_STOP, DSM_RUN, DSM_QUIT, DSM_RESTART, DSM_REBOOT, DSM_SHUTDOWN };

    bool quitCommand(enum command command)
    {
        return command == DSM_QUIT || command == DSM_REBOOT || command == DSM_SHUTDOWN;
    }

    enum command getCommand() const { return _command; }

private:

    static DSMEngine* _instance;

    /**
     * Create a signal mask, and block those masked signals.
     * DSMEngine uses sigwait, and does not register asynchronous
     * signal handlers.
     */
    void setupSignals();

    /**
     * Unblock and wait for signals of interest.
     */
    void waitForSignal();

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

    enum run_states { DSM_RUNNING, DSM_ERROR, DSM_STOPPED } _runState;

    enum command _command;

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

    std::string _username;

    uid_t _userid;

    gid_t _groupid;

    int _logLevel;

    sigset_t _signalMask;

};

}}	// namespace nidas namespace core

#endif

