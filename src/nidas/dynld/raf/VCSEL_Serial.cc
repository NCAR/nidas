/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3716 $

    $LastChangedDate: 2007-03-08 13:43:19 -0700 (Thu, 08 Mar 2007) $

    $LastChangedRevision: 3716 $

    $LastChangedBy: dongl $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/VCSEL_Serial.cc $

 ******************************************************************
*/

#include <nidas/dynld/raf/VCSEL_Serial.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

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
    DSMSerialSensor::open(flags);

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
    DSMSerialSensor::close();
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
    bool rc = DSMSerialSensor::process(samp, results);

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
    }

    return rc;
}
