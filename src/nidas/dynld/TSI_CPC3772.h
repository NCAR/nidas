/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/GPS_NMEA_Serial.h $

*/

#ifndef NIDIS_DYNLD_TSI_CPC3772_H
#define NIDIS_DYNLD_TSI_CPC3772_H

#include <nidas/dynld/DSMSerialSensor.h>

namespace nidas { namespace dynld {

/**
 * Support for a TSI CPC3772 particle counter.
 */
class TSI_CPC3772: public DSMSerialSensor
{
public:

    TSI_CPC3772():DSMSerialSensor(),_deltaTusecs(USECS_PER_SEC/10) {}

    void addSampleTag(SampleTag* stag)
            throw(nidas::util::InvalidParameterException);

    /**
     * The CPC3772 puts out a 1 Hz sample of 10 values, which
     * are the individual 10Hz samples in a second. We override
     * the process method in order to break the sample
     * into 10 individual 10 Hz samples.
     */
    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

protected:

    unsigned int _deltaTusecs;

    int _rate;
};

}}	// namespace nidas namespace dynld

#endif
