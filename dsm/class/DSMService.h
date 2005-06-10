/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_DSMSERVICE_H
#define DSM_DSMSERVICE_H

#include <atdUtil/McSocket.h>
#include <atdUtil/Thread.h>
#include <DOMable.h>
#include <SampleInput.h>
#include <SampleOutput.h>
#include <ConnectionRequester.h>

namespace dsm {

class DSMServer;
class Site;

/**
 * Base class for a service, as built from a <service> XML tag.
 */
class DSMService: public atdUtil::Thread, public SampleConnectionRequester,
	public DOMable
{
public:
    
    /**
     * Constructor.
     */
    DSMService(const std::string& name);

    virtual ~DSMService();

    /**
     * A DSMService clones itself when requests arrive.
     */
    // virtual DSMService* clone() const = 0;

    virtual void setDSMServer(DSMServer* val) { server = val; }

    virtual DSMServer* getDSMServer() const { return server; }

    virtual const Site* getSite() const;

    const std::list<const DSMConfig*>& getDSMConfigs() const
    {
        return dsms;
    }

    void addDSMConfig(const DSMConfig* val) {
        dsms.push_back(val);
    }

    /**
     * schedule this service to run.
     */
    virtual void schedule() throw(atdUtil::Exception) = 0;

    virtual int checkSubServices() throw();
    virtual void cancelSubServices() throw();
    virtual void interruptSubServices() throw();
    virtual void joinSubServices() throw();


    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    void addSubService(DSMService*) throw();

    DSMServer* server;

    std::list<const DSMConfig*> dsms;

    std::set<DSMService*> subServices;

    atdUtil::Mutex subServiceMutex;
};

}

#endif
