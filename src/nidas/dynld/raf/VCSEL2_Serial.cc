// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision$

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/dynld/raf/VCSEL2_Serial.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,VCSEL2_Serial)

VCSEL2_Serial::VCSEL2_Serial() : _atxRate(1)
{
}

VCSEL2_Serial::~VCSEL2_Serial()
{
}


void VCSEL2_Serial::open(int flags) throw(n_u::IOException)
{
    DSMSerialSensor::open(flags);

    if (DerivedDataReader::getInstance())
        DerivedDataReader::getInstance()->addClient(this);
    else
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
		getName().c_str(),
		"no DerivedDataReader. <dsm> tag needs a derivedData attribute");
}


void VCSEL2_Serial::close() throw(n_u::IOException)
{
    if (DerivedDataReader::getInstance())
	    DerivedDataReader::getInstance()->removeClient(this);
    DSMSerialSensor::close();
}


void VCSEL2_Serial::derivedDataNotify(const nidas::core::DerivedDataReader * s) throw()
{
    // std::cerr << "atx " << s->getAmbientTemperature() << std::endl;
    try {
        sendTemperaturePressure(s->getAmbientTemperature(), s->getStaticPressure());
    }
    catch(const n_u::IOException & e)
    {
        n_u::Logger::getInstance()->log(LOG_WARNING, "%s", e.what());
    }
}


void VCSEL2_Serial::sendTemperaturePressure(float atx, float psx) throw(n_u::IOException)
{
    char tmp[128];
    sprintf(tmp, "%d,%d\n", (int)(atx * 100), (int)(psx * 100));
    write(tmp, strlen(tmp));
}
