/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-03-16 23:40:37 -0600 (Fri, 16 Mar 2007) $

    $LastChangedRevision: 3736 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/DSC_FreqCounter.cc $

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
    _U0(floatNAN),_periodUsec(floatNAN),_lastSampleTime(0),
    _presSensorId(0),_presSensor(0)
{
    for (unsigned int i = 0; i < sizeof(_Y)/sizeof(_Y[0]); i++)
        _Y[i] = floatNAN;
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
    if (cf) {
        while(tt >= _calTime) {
            float d[4];
            try {
                int n = cf->readData(d,sizeof d/sizeof(d[0]));
                if (n > 0) setU0(d[0]);
                if (n > 3) setYs(0.0,d[1],d[2],d[3]);
                _calTime = cf->readTime().toUsecs();
            }
            catch(const n_u::EOFException& e)
            {
                _calTime = LONG_LONG_MAX;
            }
            catch(const n_u::Exception& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    cf->getCurrentFileName().c_str(),e.what());
                setU0(floatNAN);
                setYs(floatNAN,floatNAN,floatNAN,floatNAN);
                _calTime = LONG_LONG_MAX;
            }
        }
    }

    SampleT<float>* osamp = getSample<float>(2);
    osamp->setTimeTag(tt);
    osamp->setId(_sampleId);
    float *fp = osamp->getDataPtr();

    _periodUsec = calculatePeriodUsec(insamp) - _U0;
    _lastSampleTime = tt;

    float temp = Polynomial::eval(_periodUsec,_Y,
        sizeof(_Y)/sizeof(_Y[0]));

    *fp++ = _periodUsec;
    *fp++ = temp;
    results.push_back(osamp);

    _presSensor->createPressureSample(results);

    return true;
}

