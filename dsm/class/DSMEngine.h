/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_DSMENGINE_H
#define DSM_DSMENGINE_H

#include <Project.h>
#include <Aircraft.h>
#include <DSMConfig.h>
#include <PortSelector.h>

#include <atdUtil/Socket.h>

#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/sax/SAXException.hpp>

namespace dsm {

/**
 * Data Service Module, a class that drives an ADS3 data collection
 * box.
 */
class DSMEngine : public SampleConnectionRequester {
public:

    /**
     * Entry point to run a DSMEngine process from a command line.
     */
    static int main(int argc, char** argv) throw();

    static DSMEngine* getInstance();

    xercesc::DOMDocument* requestXMLConfig()
	throw(atdUtil::Exception,xercesc::DOMException,
		xercesc::SAXException,xercesc::XMLException);

    xercesc::DOMDocument* parseXMLConfigFile(const std::string& xmlFileName)
	throw(atdUtil::Exception,xercesc::DOMException,
		xercesc::SAXException,xercesc::XMLException);

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
    void connected(SampleInput*);

    void connected(SampleOutput*);

protected:

    /**
     * Use this static method, rather than the public constructor,
     * to create an instance of a DSMEngine which will receive signals
     * sent to a process.
     */
    static DSMEngine* createInstance();

    static DSMEngine* instance;

    static void setupSignals();

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    /**
     * Constructor.
     */
    DSMEngine();

    virtual ~DSMEngine();

    /**
     * A socket for receiving my configuration.
     */
    atdUtil::ServerSocket* streamSock;

    Project* project;
    Aircraft* aircraft;
    DSMConfig* dsmConfig;

    PortSelector* selector;

    std::list<SampleOutput*> connectedOutputs;
};

/**
 * Parse the DSM program runstring.
 */
class DSMRunstring {
public:
    DSMRunstring(int argc, char** argv);
                                                                                
    /**
     * Send usage message to cerr, then exit(1).
     */
    static void usage(const char* argv0);

    /**
     * -d option. If user wants messages on stderr rather than syslog.
     */
    bool debug;

    /**
     * Name of XML configuration file. If empty, multicast for config.
     */
    std::string configFile;
};


}

#endif

