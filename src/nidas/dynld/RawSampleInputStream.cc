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

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(RawSampleInputStream)

RawSampleInputStream::RawSampleInputStream(IOChannel* iochan):
	SampleInputStream(iochan,true)
{
}

RawSampleInputStream::RawSampleInputStream(RawSampleInputStream& x,
	IOChannel* iochannel):
	SampleInputStream(x,iochannel)
{
}

/**
 * Create a clone, with a new, connected IOChannel.
 */
RawSampleInputStream* RawSampleInputStream::clone(IOChannel* iochannel)
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
