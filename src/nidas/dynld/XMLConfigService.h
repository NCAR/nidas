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

    ~XMLConfigService();

    int run() throw(nidas::util::Exception);

    void interrupt() throw();

    void connected(IOChannel*) throw();

    void schedule() throw(nidas::util::Exception);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

private:

    /**
     * Worker thread that is run when a connection comes in.
     */
    class Worker: public nidas::util::Thread
    {
        public:
            Worker(XMLConfigService* svc,IOChannel* output,const DSMConfig* dsm);
            ~Worker();
            int run() throw(nidas::util::Exception);
        private:
            XMLConfigService* _svc;
            IOChannel* _output;
            const DSMConfig* _dsm;
    };

    /**
     * Copying not supported.
     */
    XMLConfigService(const XMLConfigService&);

    /**
     * Assignment not supported.
     */
    XMLConfigService& operator =(const XMLConfigService&);

};

}}	// namespace nidas namespace core

#endif
