/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SyncRecordOutput.h>
#include <DSMConfig.h>
#include <DSMTime.h>

#include <atdUtil/Logger.h>

#include <iostream>
#include <list>
#include <map>

using namespace dsm;
using namespace std;

CREATOR_ENTRY_POINT(SyncRecordOutput)

SyncRecordOutput::SyncRecordOutput()
{
}

SyncRecordOutput::~SyncRecordOutput()
{
}

bool SyncRecordOutput::receive(const Sample *samp)
         throw(SampleParseException, atdUtil::IOException)
{
    return true;
}

