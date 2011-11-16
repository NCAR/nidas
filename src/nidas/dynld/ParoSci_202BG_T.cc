// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/dynld/ParoSci_202BG_T.h>
#include <nidas/dynld/ParoSci_202BG_P.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(ParoSci_202BG_T)

ParoSci_202BG_T::ParoSci_202BG_T() : DSC_FreqCounter(),
    _periodUsec(floatNAN),_lastSampleTime(0),
    _presSensorId(0),_presSensor(0),_calibrator()
{
}

void ParoSci_202BG_T::readParams(const list<const Parameter*>& params)
    throw(n_u::InvalidParameterException)
{
    DSC_FreqCounter::readParams(params);
    list<const Parameter*>::const_iterator pi = params.begin();
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

void ParoSci_202BG_T::init() throw(n_u::InvalidParameterException)
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
}

float ParoSci_202BG_T::getPeriodUsec(dsm_time_t tt)
{
    if (::abs((int)(tt - _lastSampleTime)/USECS_PER_MSEC <
        getSamplePeriodMsec() / 2)) return _periodUsec;
    return floatNAN;
}

bool ParoSci_202BG_T::process(const Sample* insamp,list<const Sample*>& results)
    throw()
{   
    dsm_time_t tt = insamp->getTimeTag();
    // Read CalFile of calibration parameters.
    CalFile* cf = getCalFile();
    if (cf) _calibrator.readCalFile(cf,tt);

    SampleT<float>* osamp = getSample<float>(3);
    osamp->setTimeTag(tt);
    osamp->setId(_sampleId);
    float *fp = osamp->getDataPtr();

    _periodUsec = calculatePeriodUsec(insamp);
    float freq;
    if (_periodUsec == 0.0) freq = floatNAN;
    else freq = USECS_PER_SEC / _periodUsec;
    // cerr << "freq=" << USECS_PER_SEC / _periodUsec << " _periodUsec=" << _periodUsec << " U0=" << _calibrator.getU0() <<
      //   " usec cor=" << _periodUsec - _calibrator.getU0() << endl;

    _periodUsec -= _calibrator.getU0();
    _lastSampleTime = tt;

    float temp = _calibrator.computeTemperature(_periodUsec);
    // cerr << "_periodUsec=" << _periodUsec << " temp=" << temp << endl;

    *fp++ = _periodUsec;
    *fp++ = freq;
    *fp++ = temp;
    results.push_back(osamp);

    _presSensor->createPressureSample(results);

    return true;
}

