/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_XMLCONFIGSERVICE_H
#define DSM_XMLCONFIGSERVICE_H

#include <DSMService.h>
#include <Datagrams.h>

namespace dsm {


class XMLConfigService: public DSMService 
{
public:
    XMLConfigService();

    int run() throw(atdUtil::Exception);

    /**
     * Make a clone of myself. The ServiceListener will make
     * a clone of this service when it gets a request on a port.
     */
    atdUtil::ServiceListenerClient* clone();

    int getType() const { return XML_CONFIG; }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    void fromDOMElementOutput(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:
};

}

#endif
