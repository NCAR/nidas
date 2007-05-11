/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/CVI_LV_Input.cc $

*/

#include <nidas/dynld/raf/CVI_LV_Input.h>
#include <nidas/core/ServerSocketIODevice.h>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,CVI_LV_Input)

void CVI_LV_Input::open(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    CharacterSensor::open(flags);
    init();
}

IODevice* CVI_LV_Input::buildIODevice() throw(n_u::IOException)
{
    ServerSocketIODevice* dev = new ServerSocketIODevice();
    dev->setTcpNoDelay(true);
    return dev;
}

