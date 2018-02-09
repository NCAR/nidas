// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
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

#include "VCSEL_Serial.h"

#include <nidas/util/Logger.h>


using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,VCSEL_Serial)

VCSEL_Serial::VCSEL_Serial() : _atxRate(1), _hz_counter(0)
{
}

VCSEL_Serial::~VCSEL_Serial()
{
}


void VCSEL_Serial::open(int flags) throw(n_u::IOException)
{
    SerialSensor::open(flags);

    if (DerivedDataReader::getInstance())
        DerivedDataReader::getInstance()->addClient(this);
    else
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
		getName().c_str(),
		"no DerivedDataReader. <dsm> tag needs a derivedData attribute");
}


void VCSEL_Serial::close() throw(n_u::IOException)
{
    if (DerivedDataReader::getInstance())
	    DerivedDataReader::getInstance()->removeClient(this);
    SerialSensor::close();
}


void VCSEL_Serial::derivedDataNotify(const nidas::core::DerivedDataReader * s) throw()
{
    // std::cerr << "atx " << s->getAmbientTemperature() << std::endl;
    if (!::isnan(s->getAmbientTemperature())) {
	try {
	    sendAmbientTemperature(s->getAmbientTemperature());
	}
	catch(const n_u::IOException & e)
	{
	    n_u::Logger::getInstance()->log(LOG_WARNING, "%s", e.what());
	}
    }
}


void VCSEL_Serial::sendAmbientTemperature(float atx) throw(n_u::IOException)
{
    char tmp[128];
    sprintf(tmp, "%d\n", (int)(atx * 100));
    write(tmp, strlen(tmp));
}

bool VCSEL_Serial::process(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    bool rc = SerialSensor::process(samp, results);

    list<const Sample *>::const_iterator it = results.begin();
    for (; it != results.end(); ++it)
    {
        Sample * nco_samp = const_cast<Sample *>(*it);

        // housekeeping comes on-the-second but after the first 25Hz sample
        if ((nco_samp->getId() - getId()) == 1) {
              _hz_counter = 1;
              continue;
        }

        int usec = nco_samp->getTimeTag() % USECS_PER_SEC;

        // late samples whose timetag is in next sec are actually from previous second
        if (_hz_counter > 22 && usec < 2 * 40000) usec += USECS_PER_SEC;

        // Reverse sitution if DSM clock is a bit slow
        if (_hz_counter < 2 && usec > 22 * 40000) usec -= USECS_PER_SEC;

        dsm_time_t timeoffix = nco_samp->getTimeTag() - usec + (_hz_counter * 40000);
        if (++_hz_counter == 25) _hz_counter = 0;

        nco_samp->setTimeTag(timeoffix);

        // 1.00e+00 is no data.  Some negative values seem to get through also.
        float *values = (float *)nco_samp->getVoidDataPtr();
        if (values[0] < 10.0)
            values[0] = floatNAN;       // CONCV
    }

    return rc;
}
