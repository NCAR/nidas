// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2012-05-02 13:25:17 -0600 (Wed, 02 May 2012) $

    $LastChangedRevision: 6538 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/XMLConfigService.cc $
 ********************************************************************

*/

#include <nidas/dynld/XMLConfigAllService.h>

#include <algorithm>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(XMLConfigAllService)

XMLConfigAllService::XMLConfigAllService():
	XMLConfigService("XMLConfigAllService")
{
}

IOChannelRequester* XMLConfigAllService::connected(IOChannel* iochan) throw()
{
    // The iochan should be a new iochan, created from the configured
    // iochans, since it should be a newly connected Socket.
    // If it isn't then we have pointer ownership issues that must
    // resolved.
    list<IOChannel*>::iterator oi = std::find(_ochans.begin(),_ochans.end(),iochan);
    assert(oi == _ochans.end());

    // worker will own and delete the iochan.
    Worker* worker = new Worker(this,iochan,0);
    worker->start();
    addSubThread(worker);
    return this;
}

