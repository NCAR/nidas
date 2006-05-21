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
#include <StatusThread.h>
#include <XMLException.h>

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
     * Send usage message to cerr.
     */
    static int usage(const char* argv0);

    /**
     * Get current running instance of DSMServer.
     */
    static DSMServer* getInstance() { return serverInstance; }

    /**
     * What is the XML configuration file name.
     */
    static const std::string& getXMLFileName() { return xmlFileName; }

    /**
     * Parse an XMLConfigFile.
     */
    static Project* parseXMLConfigFile(const std::string& xmlFileName)
        throw(dsm::XMLException,atdUtil::InvalidParameterException);

protected:
    static void setupSignals();

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static int parseRunstring(int argc, char** argv);

public:

    DSMServer();

    virtual ~DSMServer();

    const std::string& getName() const { return name; }
    void setName(const std::string& val) { name = val; }

    void addService(DSMService* service) { services.push_back(service); }

    const std::list<DSMService*>& getServices() const { return services; }

    void addSite(Site* val) { sites.push_back(val); }

    const std::list<Site*>& getSites() const { return sites; }

    DSMServiceIterator getDSMServiceIterator() const;

    ProcessorIterator getProcessorIterator() const;

    SiteIterator getSiteIterator() const;

    DSMConfigIterator getDSMConfigIterator() const;

    SensorIterator getSensorIterator() const;

    SampleTagIterator getSampleTagIterator() const;

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

    /**
     * -d option. If user wants messages on stderr rather than syslog.
     */
    static bool debug;

    /**
     * The xml file that is being used for configuration information.
     */
    static std::string xmlFileName;

    /**
     * Current running instance of DSMServer.
     */
    static DSMServer* serverInstance;

    static void startStatusThread() throw(atdUtil::Exception);

    static void killStatusThread() throw(atdUtil::Exception);

    static void startXmlRpcThread() throw(atdUtil::Exception);

    static void killXmlRpcThread() throw(atdUtil::Exception);

    static bool quit;

    static bool restart;

    /** This thread that generates streaming XML time and status. */
    static DSMServerStat* _statusThread;

    /** This thread provides XML-based Remote Procedure calls */
    static DSMServerIntf* _xmlrpcThread;

    /**
     * Sites that I serve.
     */
    std::list<Site*> sites;

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
     * Copy not supported.
     */
    DSMServer(const DSMServer&);

    /**
     * Assignment not supported.
     */
    DSMServer& operator=(const DSMServer&);

};

}

#endif
