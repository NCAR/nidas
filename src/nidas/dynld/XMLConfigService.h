// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
#include <nidas/core/Datagrams.h>
#include <nidas/core/IOChannel.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

class XMLConfigService: public DSMService, public IOChannelRequester
{
public:
    XMLConfigService();

    ~XMLConfigService();

    void interrupt() throw();

    IOChannelRequester* connected(IOChannel*) throw();

    void connect(SampleInput*) throw() { assert(false); }
    void disconnect(SampleInput*) throw() { assert(false); }

    void schedule(bool optionalProcessing) throw(nidas::util::Exception);

    virtual McSocketRequest getRequestType() const 
    {
        return XML_CONFIG;
    }

protected:

    XMLConfigService(const std::string& name): DSMService(name) {}

    /**
     * Worker thread that is run when a connection comes in.
     */
    class Worker: public nidas::util::Thread
    {
        public:
            Worker(XMLConfigService* svc,IOChannel* iochan,const DSMConfig* dsm = 0);
            ~Worker();
            int run() throw(nidas::util::Exception);
            void interrupt();
        private:
            XMLConfigService* _svc;
            IOChannel* _iochan;
            const DSMConfig* _dsm;
            Worker(const Worker&);
            Worker& operator=(const Worker&);
    };

private:

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
