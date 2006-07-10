/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef NIDAS_DYNLD_XMLCONFIGSERVICE_H
#define NIDAS_DYNLD_XMLCONFIGSERVICE_H

#include <nidas/core/DSMService.h>

namespace nidas { namespace dynld {

class XMLConfigService: public DSMService, public ConnectionRequester
{
public:
    XMLConfigService();

    /**
     * Copy constructor.
     */
    XMLConfigService(const XMLConfigService&,IOChannel *iochan);

    ~XMLConfigService();

    int run() throw(nidas::util::Exception);

    void interrupt() throw();

    void connected(IOChannel*) throw();

    void schedule() throw(nidas::util::Exception);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

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

private:
    /**
     * Copy constructor.
     */
    XMLConfigService(const XMLConfigService&);

};

}}	// namespace nidas namespace core

#endif
