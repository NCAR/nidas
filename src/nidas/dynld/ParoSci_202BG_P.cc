/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-03-16 23:40:37 -0600 (Fri, 16 Mar 2007) $

    $LastChangedRevision: 3736 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/DSC_FreqCounter.cc $

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
    _U0(floatNAN),_periodUsec(floatNAN),_lastSampleTime(0),
        _tempSensorId(0),_tempSensor(0)
{
    for (unsigned int i = 0; i < sizeof(_C)/sizeof(_C[0]); i++)
        _C[i] = floatNAN;
    for (unsigned int i = 0; i < sizeof(_D)/sizeof(_D[0]); i++)
        _D[i] = floatNAN;
    for (unsigned int i = 0; i < sizeof(_T)/sizeof(_T[0]); i++)
        _T[i] = floatNAN;
}

void ParoSci_202BG_P::readParams(const list<const Parameter*>& params)
    throw(n_u::InvalidParameterException)
{
    DSC_FreqCounter::readParams(params);
    list<const Parameter*>::const_iterator pi = params.begin();
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
    return results.size() > 0;
}

void ParoSci_202BG_P::createPressureSample(list<const Sample*>& results)
{
    if (isnan(_periodUsec)) return;
    float tper = _tempSensor->getPeriodUsec(_lastSampleTime);
    if (isnan(tper)) return;
    float pper = _periodUsec;

    // Read CalFile of calibration parameters.
    CalFile* cf = getCalFile();
    if (cf) {
        while(_lastSampleTime >= _calTime) {
            float d[10];
            try {
                int n = cf->readData(d,sizeof d/sizeof(d[0]));
                if (n > 2) setCs(d[0]*MBAR_PER_PSI,d[1]*MBAR_PER_PSI,d[2]*MBAR_PER_PSI);
                if (n > 4) setDs(d[3],d[4]);
                if (n > 9) setTs(d[5],d[6],d[7],d[8],d[9]);
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
                setCs(floatNAN,floatNAN,floatNAN);
                setDs(floatNAN,floatNAN);
                setTs(floatNAN,floatNAN,floatNAN,floatNAN,floatNAN);
                _calTime = LONG_LONG_MAX;
            }
        }
    }

    double C = Polynomial::eval(tper,_C,sizeof(_C)/sizeof(_C[0]));
    double D = Polynomial::eval(tper,_D,sizeof(_D)/sizeof(_D[0]));
    double T0 = Polynomial::eval(tper,_T,sizeof(_T)/sizeof(_T[0]));

    double Tfact = (1.0 - T0 * T0 / pper / pper);
    float p = C * Tfact * (1.0 - D * Tfact);

    SampleT<float>* osamp = getSample<float>(2);
    osamp->setTimeTag(_lastSampleTime);
    osamp->setId(_sampleId);
    float *fp = osamp->getDataPtr();

    *fp++ = _periodUsec;
    *fp++ = p;
    results.push_back(osamp);

    _periodUsec = floatNAN;     // use it once
}
