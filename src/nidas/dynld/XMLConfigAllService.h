// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2012-01-30 19:22:15 -0700 (Mon, 30 Jan 2012) $

    $LastChangedRevision: 6423 $

    $LastChangedBy: wasinger $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/XMLConfigService.h $
 ********************************************************************

*/


#ifndef NIDAS_DYNLD_XMLCONFIGALLSERVICE_H
#define NIDAS_DYNLD_XMLCONFIGALLSERVICE_H

#include <nidas/dynld/XMLConfigService.h>

namespace nidas { namespace dynld {

class XMLConfigAllService: public XMLConfigService
{
public:
    XMLConfigAllService();

    nidas::core::IOChannelRequester* connected(IOChannel*) throw();

    nidas::core::McSocketRequest getRequestType() const 
    {
        return XML_ALL_CONFIG;
    }

private:

    /**
     * Copying not supported.
     */
    XMLConfigAllService(const XMLConfigService&);

    /**
     * Assignment not supported.
     */
    XMLConfigAllService& operator =(const XMLConfigService&);

};

}}	// namespace nidas namespace core

#endif
