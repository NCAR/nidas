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
#include <nidas/core/Project.h>
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

    DSMServer();

    virtual ~DSMServer();

    const std::string& getName() const { return _name; }

    void setName(const std::string& val) { _name = val; }

    const std::string& getXMLConfigFileName() const { return _xmlFileName; }

    void setXMLConfigFileName(const std::string& val) { _xmlFileName = val; }

    void addService(DSMService* service) { _services.push_back(service); }

    const std::list<DSMService*>& getServices() const { return _services; }

    void addSite(Site* val) { _sites.push_back(val); }

    const std::list<Site*>& getSites() const { return _sites; }

    DSMServiceIterator getDSMServiceIterator() const;

    ProcessorIterator getProcessorIterator() const;

    SiteIterator getSiteIterator() const;

    DSMConfigIterator getDSMConfigIterator() const;

    SensorIterator getSensorIterator() const;

    SampleTagIterator getSampleTagIterator() const;

    void addThread(nidas::util::Thread* thrd);

    void scheduleServices() throw(nidas::util::Exception);

    void interruptServices() throw();

    void joinServices() throw();

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

private:

    /**
     * Sites that I serve.
     */
    std::list<Site*> _sites;

    /**
     * Name of this server. This should correspond to a hostname
     * of a machine.
     */
    std::string _name;

    /**
     * The DSMServices that we've been configured to start.
     */
    std::list<DSMService*> _services;

    std::string _xmlFileName;

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
