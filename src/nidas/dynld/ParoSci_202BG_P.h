/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-03-04 13:00:32 -0700 (Sun, 04 Mar 2007) $

    $LastChangedRevision: 3701 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/DSC_A2DSensor.h $

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_PAROSCI_202BG_P_h
#define NIDAS_DYNLD_PAROSCI_202BG_P_h

#include <nidas/dynld/DSC_FreqCounter.h>
#include <nidas/dynld/ParoSci_202BG_Calibration.h>

using namespace nidas::core;

namespace nidas { namespace dynld {

class ParoSci_202BG_T;

/**
 * Sensor support for a ParoScientific 202BG pressure sensor
 * connected to a Diamond Systems GPIO-MM card which does
 * the necessary frequency counting.
 */
class ParoSci_202BG_P: public DSC_FreqCounter {

public:

    ParoSci_202BG_P();

    void init() throw(nidas::util::InvalidParameterException);

    /**
     * Process a raw sample, which in this case means convert the
     * counts and elapsed ticks into a signal period, then 
     * compute the pressure.
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

    /**
     * Called by the ParoSci_202GB_P and ParoSci_202GB_T process methods
     * to compute a pressure.
     */
    void createPressureSample(std::list<const Sample*>& result);

private:

    void readParams(const std::list<const Parameter*>& params)
        throw(nidas::util::InvalidParameterException);


    float _periodUsec;

    dsm_time_t _lastSampleTime;

    dsm_sample_id_t _tempSensorId;

    ParoSci_202BG_T* _tempSensor;

    ParoSci_202BG_Calibration _calibrator;

};

}}	// namespace nidas namespace dynld

#endif
