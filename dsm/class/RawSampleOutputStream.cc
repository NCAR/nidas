/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <RawSampleOutputStream.h>
#include <Datagrams.h>

#include <atdUtil/Logger.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(RawSampleOutputStream)

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


void RawSampleOutputStream::fromDOMElement(const DOMElement* node)
        throw(atdUtil::InvalidParameterException)
{
    SortedSampleOutputStream::fromDOMElement(node);
    if (getIOChannel()->getRequestNumber() < 0)
    	getIOChannel()->setRequestNumber(RAW_SAMPLE);
}
