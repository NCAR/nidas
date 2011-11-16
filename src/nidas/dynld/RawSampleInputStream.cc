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

#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/core/IOChannel.h>

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(RawSampleInputStream)

RawSampleInputStream::RawSampleInputStream(): SampleInputStream(true)
{
}

RawSampleInputStream::RawSampleInputStream(nidas::core::IOChannel* iochan):
	SampleInputStream(iochan,true)
{
}

RawSampleInputStream::RawSampleInputStream(RawSampleInputStream& x,
	nidas::core::IOChannel* iochannel):
	SampleInputStream(x,iochannel)
{
}

/**
 * Create a clone, with a new, connected IOChannel.
 */
RawSampleInputStream* RawSampleInputStream::clone(nidas::core::IOChannel* iochannel)
{
    return new RawSampleInputStream(*this,iochannel);
}


RawSampleInputStream::~RawSampleInputStream()
{
}

void RawSampleInputStream::fromDOMElement(const xercesc::DOMElement* node)
        throw(n_u::InvalidParameterException)
{
    SampleInputStream::fromDOMElement(node);
    if (_iochan->getRequestType() < 0)
    	_iochan->setRequestType(RAW_SAMPLE);
}
