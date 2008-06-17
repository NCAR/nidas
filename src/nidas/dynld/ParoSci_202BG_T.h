/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-03-04 13:00:32 -0700 (Sun, 04 Mar 2007) $

    $LastChangedRevision: 3701 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/DSC_A2DSensor.h $

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_PAROSCI_202BG_T_h
#define NIDAS_DYNLD_PAROSCI_202BG_T_h

#include <nidas/dynld/DSC_FreqCounter.h>

using namespace nidas::core;

namespace nidas { namespace dynld {

class ParoSci_202BG_P;

/**
 * Sensor support for a ParoScientific 202BG temperature sensor
 * connected to a Diamond Systems GPIO-MM card which does
 * the necessary frequency counting.
 */
class ParoSci_202BG_T: public DSC_FreqCounter {

public:

    ParoSci_202BG_T();

    void init() throw(nidas::util::InvalidParameterException);

    void setU0(float val) { _U0 = val; }

    void setYs(float v0, float v1, float v2, float v3)
    {
        _Y[0] = v0; _Y[1] = v1; _Y[2] = v2; _Y[3] = v3;
    }

    /**
     * Process a raw sample, which in this case means convert the
     * counts and elapsed ticks into a frequency.
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

    /**
     * Get the most recent period in microseconds of the input signal.
     * Returns floatNAN if the time is more than half a sample period
     * away from the most recent sample.
     */
    float getPeriodUsec(dsm_time_t tt);


private:

    void readParams(const std::list<const Parameter*>& params)
        throw(nidas::util::InvalidParameterException);

    float _U0;

    float _Y[4];

    dsm_time_t _calTime;

    float _periodUsec;

    dsm_time_t _lastSampleTime;

    dsm_sample_id_t _presSensorId;

    ParoSci_202BG_P* _presSensor;

};

}}	// namespace nidas namespace dynld

#endif
