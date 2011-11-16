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
#ifndef NIDAS_DYNLD_PAROSCI_202BG_T_h
#define NIDAS_DYNLD_PAROSCI_202BG_T_h

#include <nidas/dynld/DSC_FreqCounter.h>
#include <nidas/dynld/ParoSci_202BG_Calibration.h>

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

    float _periodUsec;

    dsm_time_t _lastSampleTime;

    dsm_sample_id_t _presSensorId;

    ParoSci_202BG_P* _presSensor;

    ParoSci_202BG_Calibration _calibrator;

    /** No copying. */
    ParoSci_202BG_T(const ParoSci_202BG_T&);

    /** No assignment. */
    ParoSci_202BG_T& operator=(const ParoSci_202BG_T&);

};

}}	// namespace nidas namespace dynld

#endif
