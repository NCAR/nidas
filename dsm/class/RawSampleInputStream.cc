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

CREATOR_ENTRY_POINT(RawSampleInputStream)

RawSampleInputStream::RawSampleInputStream(IOChannel* iochannel):
	SampleInputStream(iochannel)
{
}

RawSampleInputStream::~RawSampleInputStream()
{
}

