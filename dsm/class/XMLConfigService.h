/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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

    void connected(IOChannel*) throw();

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

    /**
     * After a DSM connects, and the XMLConfigService is cloned,
     * then this method is called to set the DSMConfig,  so that
     * the Thread::run() method delivers XML for a specific DSM.
     */
    void setDSMConfig(const DSMConfig* val) { dsm = val; }

    const DSMConfig* getDSMConfig() const { return dsm; }

    IOChannel* iochan;

    const DSMConfig* dsm;

};

}

#endif
