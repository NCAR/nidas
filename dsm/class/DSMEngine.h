/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef DSM_DSMENGINE_H
#define DSM_DSMENGINE_H

#include <Project.h>
#include <Site.h>
#include <DSMConfig.h>
#include <PortSelector.h>
#include <StatusThread.h>
#include <XMLConfigInput.h>
#include <DSMEngineIntf.h>
#include <XMLException.h>

#include <atdUtil/Socket.h>
#include <atdUtil/Logger.h>

#include <xercesc/dom/DOMDocument.hpp>

namespace dsm {

class DSMRunstring;

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
    void run() throw();

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
    void mainStart();

    /** Stops the main loop (for the XMLRPC call). */
    void mainStop();

    /** Restarts the main loop (for the XMLRPC call). */
    void mainRestart();

    /** Quits the main loop (for the XMLRPC call). */
    void mainQuit();

    SampleDater* getSampleDater() { return &_dater; }

    const DSMConfig* getDSMConfig() const { return _dsmConfig; }

    const PortSelector* getPortSelector() const { return _selector; }

    /**
     * Request the XML configuration via a McSocket request to
     * a given multicast socket address.
     */
    xercesc::DOMDocument* requestXMLConfig(const atdUtil::Inet4SocketAddress&)
	throw(atdUtil::Exception);

    /**
     * This function is used as a utility and does not need an instance
     * of DSMEngine.
     */
    static xercesc::DOMDocument* parseXMLConfigFile(const std::string& xmlFileName)
	throw(dsm::XMLException);

protected:

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
            throw(atdUtil::InvalidParameterException);

    void openSensors() throw(atdUtil::IOException);

    void connectOutputs() throw(atdUtil::IOException);

    void wait() throw(atdUtil::Exception);

    void interrupt() throw(atdUtil::Exception);

    /**
     * Implementation of ConnectionRequester connected methods.
     * This is how DSMEngine is notified of remote connections.
     */
    void connected(SampleOutput*) throw();

    void disconnected(SampleOutput*) throw();

    static DSMEngine* _instance;

    /**
     * Whether to log messages on syslog (true) or stderr (false).
     * Set to false from -d runstring option, otherwise true.
     */
    bool _syslogit;

    /** -w runstring option. If user wants to wait for the XmlRpc
     * 'start' cammand. */
    bool _wait;

    /** Name of XML configuration file. If empty, multicast for config. */
    std::string _configFile;

    /**
     * Multicast address to use when fishing for the XML configuration.
     */
    atdUtil::Inet4SocketAddress _mcastSockAddr;

    /**  main loop "thread" control flags. */
    bool          _run;
    bool          _quit;
    bool          _interrupt;
    atdUtil::Cond _runCond;

    static void setupSignals();

    /** Signal handler */
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    Project*         _project;

    DSMConfig*       _dsmConfig;

    PortSelector*    _selector;

    /**
     * A thread that generates streaming XML time and status.
     */
    DSMEngineStat*    _statusThread;

    /** This thread provides XML-based Remote Procedure calls */
    DSMEngineIntf*   _xmlrpcThread;

    SampleDater      _dater;

    std::list<SampleOutput*>  _connectedOutputs;

    atdUtil::Mutex            _outputMutex;

    XMLConfigInput*           _xmlRequestSocket;

    atdUtil::Logger*          _logger;

};

}

#endif

