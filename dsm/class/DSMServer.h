/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_DSMSERVER_H
#define DSM_DSMSERVER_H

#include <DSMService.h>
#include <Project.h>
#include <DOMable.h>

#include <xercesc/sax/SAXException.hpp>

#include <list>

namespace dsm {

/**
 * A provider of services to a DSM.
 */
class DSMServer: public DOMable {
public:

    /**
    * Run a server from the unix command line.
    */
    static int main(int argc, char** argv) throw();

    /**
     * When a server is run from the command line, this is the name
     * of the XML file that it is running.
     */
    static const std::string& getXMLFileName() { return xmlFileName; }

    static void setXMLFileName(const std::string& val) { xmlFileName = val; }

protected:
    static void setupSignals();

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    /**
     * Parse an XMLConfigFile. This is a static method called
     * from main(), that is when a server is actually run.
     */
    static Project* parseXMLConfigFile()
        throw(atdUtil::Exception,xercesc::DOMException,
	xercesc::SAXException,xercesc::XMLException,
	atdUtil::InvalidParameterException);

public:

    DSMServer();

    /**
     * Copy constructor.
     */
    DSMServer(const DSMServer&);

    virtual ~DSMServer();

    void setAircraft(const Aircraft* val) { aircraft = val; }
    const Aircraft* getAircraft() const { return aircraft; }

    const std::string& getName() const { return name; }
    void setName(const std::string& val) { name = val; }

    void addService(DSMService* service) { services.push_back(service); }
    // const std::list<DSMService*>& getServices() const { return services; }
                                                                                
    void addThread(atdUtil::Thread* thrd);

    void scheduleServices() throw(atdUtil::Exception);

    void waitOnServices() throw();
    void interruptServices() throw();
    void cancelServices() throw();
    void joinServices() throw();

    void fromDOMElement(const xercesc::DOMElement*)
        throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    const Aircraft* aircraft;

    static std::string xmlFileName;
    static bool quit;
    static bool restart;

    /**
     * Name of this server. This should correspond to a hostname
     * of a machine.
     */
    std::string name;

    /**
     * The DSMServices that we've been configured to start.
     */
    std::list<DSMService*> services;

};

/**
 * Class for parse the server program runstring.
 */
class DSMServerRunstring {
public:
    DSMServerRunstring(int argc, char** argv);
                                                                                
    /**
     * Send usage message to cerr, then exit(1).
     */
    static void usage(const char* argv0);
                                                                                
    /**
     * -d option. If user wants messages on stderr rather than syslog.
     */
    bool debug;
                                                                                
    /**
     * Name of XML configuration file. Required.
     */
    std::string configFile;
};

}

#endif
