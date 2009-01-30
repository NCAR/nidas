/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef NIDAS_CORE_DSMSERVER_H
#define NIDAS_CORE_DSMSERVER_H

#include <nidas/core/DSMService.h>
#include <nidas/core/DSMServerIntf.h>
#include <nidas/core/Project.h>
#include <nidas/core/ProjectConfigs.h>
#include <nidas/core/DOMable.h>
#include <nidas/core/StatusThread.h>
#include <nidas/core/XMLException.h>

#include <list>

namespace nidas { namespace core {

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
        throw(nidas::core::XMLException,nidas::util::InvalidParameterException,nidas::util::IOException);

    static const char* rafXML;

    static const char* isffXML;

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

    void addThread(nidas::util::Thread* thrd);

    void scheduleServices() throw(nidas::util::Exception);

    void waitOnServices() throw();
    void interruptServices() throw();
    void cancelServices() throw();
    void joinServices() throw();

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

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
     * The xml file that contains all the project configurations, by date.
     */
    static std::string configsXMLName;

    /**
     * Current running instance of DSMServer.
     */
    static DSMServer* serverInstance;

    static void startStatusThread() throw(nidas::util::Exception);

    static void killStatusThread() throw(nidas::util::Exception);

    static void startXmlRpcThread() throw(nidas::util::Exception);

    static void killXmlRpcThread() throw(nidas::util::Exception);

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

    static std::string _username;

    static uid_t _userid;

    static gid_t _groupid;

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

}}	// namespace nidas namespace core

#endif
