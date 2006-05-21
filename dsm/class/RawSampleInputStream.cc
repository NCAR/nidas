/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <RawSampleInputStream.h>

#include <atdUtil/Logger.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(RawSampleInputStream)

RawSampleInputStream::RawSampleInputStream(IOChannel* iochan):
	SampleInputStream(iochan)
{
}

RawSampleInputStream::RawSampleInputStream(const RawSampleInputStream& x,
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

void RawSampleInputStream::fromDOMElement(const DOMElement* node)
        throw(atdUtil::InvalidParameterException)
{
    SampleInputStream::fromDOMElement(node);
    if (iochan->getRequestNumber() < 0)
    	iochan->setRequestNumber(RAW_SAMPLE);
}
