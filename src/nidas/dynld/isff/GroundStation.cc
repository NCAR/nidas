/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-25 12:22:19 -0600 (Tue, 25 Oct 2005) $

    $LastChangedRevision: 3073 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Aircraft.cc $
 ********************************************************************

*/

#include <nidas/dynld/isff/GroundStation.h>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(isff,GroundStation)

GroundStation::GroundStation():Site()
{
}

GroundStation::~GroundStation()
{
}

