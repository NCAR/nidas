/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/GPS_NMEA_Serial.cc $

*/

#include <nidas/dynld/TSI_CPC3772.h>

#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(TSI_CPC3772)

void TSI_CPC3772::addSampleTag(SampleTag* stag)
	throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::addSampleTag(stag);
    if (getSampleTags().size() > 1)
        throw n_u::InvalidParameterException(getName(),
            "addSampleTag","does not support more than 1 sample tag");

    if (stag->getRate() > 0.0) {
        _deltaTusecs = (int)rint(USECS_PER_SEC / stag->getRate());
        _rate = (int)rint(stag->getRate());
    }

}

bool TSI_CPC3772::process(const Sample* samp,list<const Sample*>& results)
	throw()
{

    list<const Sample*> tmpResults;
    DSMSerialSensor::process(samp,tmpResults);

    list<const Sample*>::const_iterator si = tmpResults.begin();
    for ( ; si != tmpResults.end(); ++si) {
        const Sample* isamp = *si;
        if (isamp->getType() == FLOAT_ST && isamp->getDataLength() == _rate) {
            const float* ifp = (const float*) isamp->getConstVoidDataPtr();
            for (unsigned int i = isamp->getDataLength(); i-- > 0; ) {
                SampleT<float>* osamp = getSample<float>(1);
                osamp->setTimeTag(isamp->getTimeTag() - i * _deltaTusecs);
                osamp->setId(isamp->getId());
                *osamp->getDataPtr() = *ifp++;
                results.push_back(osamp);
            }
        }
        isamp->freeReference();     // done with it.
    }
    return results.size() > 0;
}

