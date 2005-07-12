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
#include <XmlRpcThread.h>

#include <atdUtil/Socket.h>
#include <atdUtil/Logger.h>

#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/sax/SAXException.hpp>

namespace dsm {

class DSMRunstring;

/**
 * A singleton class that drives an ADS3 data collection box.
 */
class DSMEngine : public SampleConnectionRequester {
public:

    /**
     * Entry point to run a DSMEngine process from a command line.
     */
    static int main(int argc, char** argv) throw();

    /** Starts the main loop (for the XMLRPC call). */
    void mainStart();

    /** Stops the main loop (for the XMLRPC call). */
    void mainStop();

    /** Restarts the main loop (for the XMLRPC call). */
    void mainRestart();

    /** Quits the main loop (for the XMLRPC call). */
    void mainQuit();

    static DSMEngine* getInstance();

    virtual ~DSMEngine();

    SampleDater* getSampleDater() { return &_dater; }

    const DSMConfig* getDSMConfig() const { return _dsmConfig; }

    const PortSelector* getPortSelector() const { return _selector; }

    /**
     * Request the XML configuration via a McSocket request to
     * a given multicast socket address.
     */
    xercesc::DOMDocument* requestXMLConfig(const atdUtil::Inet4SocketAddress&)
	throw(atdUtil::Exception,xercesc::DOMException,
		xercesc::SAXException,xercesc::XMLException);

    /**
     * This function is used as a utility and does not need an instance
     * of DSMEngine.
     */
    static xercesc::DOMDocument* parseXMLConfigFile(const std::string& xmlFileName)
	throw(atdUtil::Exception,xercesc::DOMException,
		xercesc::SAXException,xercesc::XMLException);

protected:
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

    /** main loop */
    void run() throw();

    /**  main loop "thread" control flags. */
    bool          _run;
    bool          _quit;
    bool          _interrupt;
    atdUtil::Cond _runCond;

    /**
     * Use this static method, rather than the public constructor,
     * to create an instance of a DSMEngine which will receive signals
     * sent to a process.
     */
    static DSMEngine* createInstance(const DSMRunstring* rstr);

    static DSMEngine* _instance;

    static void setupSignals();

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    DSMEngine(const DSMRunstring* rstr);

    Project*         _project;
    DSMConfig*       _dsmConfig;
    PortSelector*    _selector;
    StatusThread*    _statusThread;

    /** This thread provides XML-based Remote Procedure calls. */
    XmlRpcThread*    _xmlrpcThread;

    SampleDater      _dater;

    std::list<SampleOutput*>  _connectedOutputs;
    atdUtil::Mutex            _outputMutex;
    XMLConfigInput*           _xmlRequestSocket;
    atdUtil::Logger*          _logger;
    const DSMRunstring*       _rstr;
};

/**
 * Parse the DSM program runstring.
 */
class DSMRunstring {
 public:
  DSMRunstring(int argc, char** argv);
                                                                                
  /** Send usage message to cerr, then exit(1). */
  static void usage(const char* argv0);

  /** -d option. If user wants messages on stderr rather than syslog. */
  bool _debug;

  /** -w option. If user wants to wait for the XmlRpc 'start' cammand. */
  bool _wait;

  /** Name of XML configuration file. If empty, multicast for config. */
  std::string _configFile;

  atdUtil::Inet4SocketAddress _mcastSockAddr;
};

}

#endif

