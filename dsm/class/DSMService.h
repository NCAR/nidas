/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_DSMSERVICE_H
#define DSM_DSMSERVICE_H

#include <atdUtil/McSocket.h>
#include <atdUtil/Thread.h>
#include <DOMable.h>
#include <SampleInput.h>
#include <SampleOutput.h>

namespace dsm {

class DSMServer;
class Aircraft;

/**
 * Base class for a service, as built from a <service> XML tag.
 */
class DSMService: public atdUtil::Thread, public DOMable
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
    virtual DSMService* clone() const = 0;

    void setServer(DSMServer* val) { server = val; }
    DSMServer* getServer() const { return server; }

    const Aircraft* getAircraft() const;

    /**
     * Derived classes may override this if they need to
     * set the DSMConfig value of their inputs or outputs.
     */
    virtual void setDSMConfig(const DSMConfig* val) { dsm = val; }

    virtual const DSMConfig* getDSMConfig() const { return dsm; }

    /**
     * schedule this service to run.
     */
    virtual void schedule() throw(atdUtil::Exception) = 0;

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    DSMServer* server;

    const DSMConfig* dsm;

};

}

#endif
