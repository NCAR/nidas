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

#include "ParoSci_202BG_T.h"
#include "ParoSci_202BG_P.h"
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(ParoSci_202BG_T)

ParoSci_202BG_T::ParoSci_202BG_T() : DSC_FreqCounter(),
    _periodUsec(floatNAN),_sampleTime(0),
    _presSensorId(0),_presSensor(0),_calibrator(),
    _calfile(0)
{

}

void ParoSci_202BG_T::readParams(const list<const Parameter*>& params)
{
    DSC_FreqCounter::readParams(params);
    list<const Parameter*>::const_iterator pi;
    for (pi = params.begin(); pi != params.end(); ++pi) {
        const Parameter* p = *pi;
        if (p->getName() == "PresSensor") {
            if (p->getType() != Parameter::INT_PARAM || p->getLength() != 2)
                throw n_u::InvalidParameterException(getName(),
                    "PresSensor sensor Id","should be a integer of length 2");
             int dsmid = (int)rint(p->getNumericValue(0));
             int sensorid = (int)rint(p->getNumericValue(1));
             _presSensorId = 0;
             _presSensorId = SET_DSM_ID(_presSensorId,dsmid);
             _presSensorId = SET_SHORT_ID(_presSensorId,sensorid);
        }
    }
}

void ParoSci_202BG_T::init()
{
    DSC_FreqCounter::init();
    _presSensor = dynamic_cast<ParoSci_202BG_P*>
        (Project::getInstance()->findSensor(_presSensorId));
    if (!_presSensor) {
        ostringstream ost;
        ost << "cannot find ParoSci_202BG_P sensor with id=" <<
            GET_DSM_ID(_presSensorId) << ',' <<
            GET_SHORT_ID(_presSensorId);
        throw n_u::InvalidParameterException(getName(),
                    "PresSensor sensor Id",ost.str());
    }
    _calfile = getCalFile("");
}

float ParoSci_202BG_T::getPeriodUsec(dsm_time_t tt)
{
    if (::abs((int)((tt - _sampleTime)/USECS_PER_MSEC)) <
        getSamplePeriodMsec() / 2) return _periodUsec;
    return floatNAN;
}

bool ParoSci_202BG_T::process(const Sample* insamp,list<const Sample*>& results)
    throw()
{   
    _sampleTime = insamp->getTimeTag() - getLagUsecs();
    TimetagAdjuster* ttadj = _ttadjusters[_stag];
    if (ttadj)
    {
        _sampleTime = ttadj->adjust(_sampleTime);
    }

    // Read CalFile of calibration parameters.
    try {
        if (_calfile) _calibrator.readCalFile(_calfile,_sampleTime);
    }
    catch(const n_u::Exception& e) {
        _calfile = 0;
    }

    SampleT<float>* osamp = getSample<float>(3);
    osamp->setTimeTag(_sampleTime);
    osamp->setId(_sampleId);

    float *fp = osamp->getDataPtr();

    _periodUsec = calculatePeriodUsec(insamp);
    float freq;
    if (_periodUsec == 0.0) freq = floatNAN;
    else freq = USECS_PER_SEC / _periodUsec;
    // cerr << "freq=" << USECS_PER_SEC / _periodUsec << " _periodUsec=" << _periodUsec << " U0=" << _calibrator.getU0() <<
      //   " usec cor=" << _periodUsec - _calibrator.getU0() << endl;

    _periodUsec -= _calibrator.getU0();

    float temp = _calibrator.computeTemperature(_periodUsec);
    // cerr << "_periodUsec=" << _periodUsec << " temp=" << temp << endl;

    *fp++ = _periodUsec;
    *fp++ = freq;
    *fp++ = temp;

    results.push_back(osamp);

    // Originally _presSensor->createPressureSample(results)
    // was called here.  I now don't think it's necessary, it
    // is called by the pressure sensor on receipt of a pressure
    // sample, which then calls getPeriodUsec(_sampleTime) on 
    // this temperature sensor.
    // _presSensor->createPressureSample(results);

    return true;
}

