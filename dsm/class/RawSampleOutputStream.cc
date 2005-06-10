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

#include <atdUtil/Logger.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(RawSampleOutputStream)

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

SampleOutput* RawSampleOutputStream::clone(IOChannel* iochannel) const 
{
    return new RawSampleOutputStream(*this,iochannel);
}

