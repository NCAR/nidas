// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
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
