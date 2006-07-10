/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/dynld/RawSampleOutputStream.h>
#include <nidas/core/Datagrams.h>

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(RawSampleOutputStream)

RawSampleOutputStream::RawSampleOutputStream()
{
}

RawSampleOutputStream::RawSampleOutputStream(const RawSampleOutputStream& x):
	SortedSampleOutputStream(x)
{
}

RawSampleOutputStream::RawSampleOutputStream(const RawSampleOutputStream& x,
	IOChannel* iochannel):
	SortedSampleOutputStream(x,iochannel)
{
}

RawSampleOutputStream::~RawSampleOutputStream()
{
}

RawSampleOutputStream* RawSampleOutputStream::clone(IOChannel* iochannel) const 
{
    return new RawSampleOutputStream(*this,iochannel);
}


void RawSampleOutputStream::fromDOMElement(const xercesc::DOMElement* node)
        throw(n_u::InvalidParameterException)
{
    SortedSampleOutputStream::fromDOMElement(node);
    if (getIOChannel()->getRequestNumber() < 0)
    	getIOChannel()->setRequestNumber(RAW_SAMPLE);
}
