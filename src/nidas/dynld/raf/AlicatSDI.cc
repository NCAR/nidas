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

#include "AlicatSDI.h"

#include <nidas/core/PhysConstants.h>
#include <nidas/util/Logger.h>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, AlicatSDI)

AlicatSDI::AlicatSDI() :
    _nTASav(5), _tas(0.0), _tasIdx(0), _tasWeight(0),
    _Qmin(100), _Qmax(500), _Qfac(0.0)
{

}

AlicatSDI::~AlicatSDI()
{
    delete [] _tasWeight;
}


void AlicatSDI::validate() throw(n_u::InvalidParameterException)
{
    SerialSensor::validate();

    const Parameter* param;

    param = getParameter("nTAS_AVERAGE");
    if (!param) throw n_u::InvalidParameterException(getName(),
          "nTAS_AVERAGE","not found");
    _nTASav = (int)param->getNumericValue(0);
    if (_nTASav < 1) throw n_u::InvalidParameterException(getName(),
          "nTAS_AVERAGE","needs to be greater than 0");

    param = getParameter("QMIN");
    if (!param) throw n_u::InvalidParameterException(getName(),
          "QMIN","not found");
    _Qmin = (int)param->getNumericValue(0);

    param = getParameter("QMAX");
    if (!param) throw n_u::InvalidParameterException(getName(),
          "QMAX","not found");
    _Qmax = (int)param->getNumericValue(0);

    param = getParameter("TIP_DIAM");
    if (!param) throw n_u::InvalidParameterException(getName(),
          "TIP_DIAM","not found");
    float tipDiam = param->getNumericValue(0);


    _tas.clear();
    float w[_nTASav], wSum = 0.0;
    for (int i = 0; i < _nTASav; ++i) {
        _tas.push_back(0.0);
        w[i] = exp((float)-i / (_nTASav-1));
        wSum += w[i];
    }
    _tasWeight = new float[_nTASav];
    for (int i = 0; i < _nTASav; ++i)
        _tasWeight[i] = w[i] / wSum;

    _Qfac = 100.0 * M_PI * pow(tipDiam / 2.0, 2.0) * 60.0 / 1000.0;
}

void AlicatSDI::open(int flags) throw(n_u::IOException)
{
    SerialSensor::open(flags);

    if (DerivedDataReader::getInstance())
        DerivedDataReader::getInstance()->addClient(this);
    else
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
		getName().c_str(),
		"no DerivedDataReader. <dsm> tag needs a derivedData attribute");


// Initialize instrument here
    struct timespec nsleep;

    write("AHC\r", 4);   // Hold valve closed
    nsleep.tv_sec = 1;
    nsleep.tv_nsec = 0;
    ::nanosleep(&nsleep, 0);    // sleep for 1 second.
    write("AV\r", 3);    // Tare the Alicat

    nsleep.tv_sec = 0;
    nsleep.tv_nsec = NSECS_PER_SEC / 10;                // 1/10th sec
    ::nanosleep(&nsleep, 0);

    write("AC\r", 3);   // Cancel hold

    char msg[32];
    sprintf(msg, "AS%d\r", _Qmin);
    write(msg, strlen(msg));
}


void AlicatSDI::close() throw(n_u::IOException)
{
    if (DerivedDataReader::getInstance())
	    DerivedDataReader::getInstance()->removeClient(this);
    SerialSensor::close();
}


void AlicatSDI::derivedDataNotify(const nidas::core::DerivedDataReader * s) throw()
{
    try {
        if ( !isnan(s->getTrueAirspeed()) ) {
            _tas[_tasIdx++] = s->getTrueAirspeed();
            if (_tasIdx >= _nTASav) _tasIdx = 0;
        }
        if ( !isnan(s->getStaticPressure()) )
            _ps = s->getStaticPressure();
        if ( !isnan(s->getAmbientTemperature()) )
            _at = s->getAmbientTemperature();

        float flow = computeFlow();
        sendFlow(flow);
    }
    catch(const n_u::IOException & e)
    {
        n_u::Logger::getInstance()->log(LOG_WARNING, "%s", e.what());
    }
}


float AlicatSDI::computeFlow()
{
    float tasSum = 0.0;
    int tasidx = _tasIdx;
    for (int i = 0; i < _nTASav; ++i)
    {
        if (--tasidx < 0) tasidx = _nTASav - 1;
        tasSum += _tas[tasidx] * _tasWeight[i];
    }

    float Qiso = _Qfac * (_ps / STANDARD_ATMOSPHERE) * Tstd / (_at + KELVIN_AT_0C) * tasSum;

    if (Qiso < _Qmin || isnan(Qiso)) Qiso = _Qmin;
    else
    if (Qiso > _Qmax) Qiso = _Qmax;

    return Qiso;
}


void AlicatSDI::sendFlow(float flow) throw(n_u::IOException)
{
    char tmp[128];
    sprintf(tmp, "<AS>%.1f\r", flow);
    write(tmp, strlen(tmp));
}
