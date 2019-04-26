// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2013, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "VCSEL2_Serial.h"

#include <nidas/util/Logger.h>

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
    SerialSensor::open(flags);

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
    SerialSensor::close();
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
