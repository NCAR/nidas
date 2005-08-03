/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_DSMSERVER_H
#define DSM_DSMSERVER_H

#include <DSMService.h>
#include <DSMServerIntf.h>
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

    virtual ~DSMServer();

    void setSite(const Site* val) { site = val; }
    const Site* getSite() const { return site; }

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

    static void startXmlRpcThread() throw(atdUtil::Exception);
    static void killXmlRpcThread() throw(atdUtil::Exception);

    const Site* site;

    static std::string xmlFileName;
    static bool quit;
    static bool restart;

    /** This thread provides XML-based Remote Procedure calls */
    static DSMServerIntf* _xmlrpcThread;

    /**
     * Name of this server. This should correspond to a hostname
     * of a machine.
     */
    std::string name;

    /**
     * The DSMServices that we've been configured to start.
     */
    std::list<DSMService*> services;

private:
    /**
     * Copy constructor.
     */
    DSMServer(const DSMServer&);

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
