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

#include <nidas/dynld/ParoSci_202BG_P.h>
#include <nidas/dynld/ParoSci_202BG_T.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>
#include <nidas/core/PhysConstants.h>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(ParoSci_202BG_P)

ParoSci_202BG_P::ParoSci_202BG_P() : DSC_FreqCounter(),
    _periodUsec(floatNAN),_lastSampleTime(0),
    _tempSensorId(0),_tempSensor(0),_calibrator()
{
}

void ParoSci_202BG_P::readParams(const list<const Parameter*>& params)
    throw(n_u::InvalidParameterException)
{
    DSC_FreqCounter::readParams(params);
    list<const Parameter*>::const_iterator pi;
    for (pi = params.begin(); pi != params.end(); ++pi) {
        const Parameter* p = *pi;
        if (p->getName() == "TempSensor") {
            if (p->getType() != Parameter::INT_PARAM || p->getLength() != 2)
                throw n_u::InvalidParameterException(getName(),
                    "TempSensor sensor Id","should be a integer of length 2");
             int dsmid = (int)rint(p->getNumericValue(0));
             int sensorid = (int)rint(p->getNumericValue(1));
             _tempSensorId = 0;
             _tempSensorId = SET_DSM_ID(_tempSensorId,dsmid);
             _tempSensorId = SET_SHORT_ID(_tempSensorId,sensorid);
        }
    }
}

void ParoSci_202BG_P::init() throw(n_u::InvalidParameterException)
{
    DSC_FreqCounter::init();
    _tempSensor = dynamic_cast<ParoSci_202BG_T*>
        (Project::getInstance()->findSensor(_tempSensorId));
    if (!_tempSensor) {
        ostringstream ost;
        ost << "cannot find ParoSci_202BG_T sensor with id=" <<
            GET_DSM_ID(_tempSensorId) << ',' <<
            GET_SHORT_ID(_tempSensorId);
        throw n_u::InvalidParameterException(getName(),
                    "TempSensor sensor Id",ost.str());
    }
}

bool ParoSci_202BG_P::process(const Sample* insamp,list<const Sample*>& results)
    throw()
{   
    _lastSampleTime = insamp->getTimeTag();
    _periodUsec = calculatePeriodUsec(insamp);

    createPressureSample(results);
    return !results.empty();
}

void ParoSci_202BG_P::createPressureSample(list<const Sample*>& results)
{
    if (isnan(_periodUsec)) return;
    float tper = _tempSensor->getPeriodUsec(_lastSampleTime);
    if (isnan(tper)) return;
    float pper = _periodUsec;

    // Read CalFile of calibration parameters.
    CalFile* cf = getCalFile();
    if (cf) _calibrator.readCalFile(cf,_lastSampleTime);

    float p = _calibrator.computePressure(tper,pper);

    SampleT<float>* osamp = getSample<float>(3);
    osamp->setTimeTag(_lastSampleTime);
    osamp->setId(_sampleId);
    float *fp = osamp->getDataPtr();

    *fp++ = pper;
    if (pper == 0.0) *fp++ = floatNAN;
    else *fp++ = USECS_PER_SEC / pper;
    *fp++ = p;
    results.push_back(osamp);

    _periodUsec = floatNAN;     // use it once
}
