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

namespace dsm {

class XMLConfigService: public DSMService, public ConnectionRequester
{
public:
    XMLConfigService();

    /**
     * Copy constructor.
     */
    XMLConfigService(const XMLConfigService&);

    ~XMLConfigService();

    DSMService* clone() const;

    int run() throw(atdUtil::Exception);

    void connected(IOChannel*);

    void schedule() throw(atdUtil::Exception);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:
    IOChannel* iochan;
};

}

#endif
