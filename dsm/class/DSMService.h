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

#include <atdUtil/ServiceListenerClient.h>
#include <SampleInputStream.h>
#include <SampleOutputStream.h>
#include <DOMable.h>

namespace dsm {

/**
 * Base class for a service, as built from a <service> XML tag.
 * A service run() method should do a
 *	::kill(0,SIGUSR1);
 * just before exiting, to give the server a timely notice
 * that the thread is finished.
 */
class DSMService:  public atdUtil::ServiceListenerClient, public DOMable
{
public:
    
    /**
     * Constructor.
     */
    DSMService(const std::string& name):
    	atdUtil::ServiceListenerClient(name)
    {
        blockSignal(SIGUSR1);
    }

    virtual ~DSMService() {}

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);


protected:

    std::list<SampleInputStream*> inputStreams;
    std::list<SampleOutputStream*> outputStreams;
};

}

#endif
