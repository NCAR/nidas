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
#ifndef NIDAS_DYNLD_PAROSCI_202BG_P_h
#define NIDAS_DYNLD_PAROSCI_202BG_P_h

#include "DSC_FreqCounter.h"
#include "ParoSci_202BG_Calibration.h"

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

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void init();

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

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void readParams(const std::list<const Parameter*>& params);

    float _periodUsec;

    dsm_time_t _sampleTime;

    dsm_sample_id_t _tempSensorId;

    ParoSci_202BG_T* _tempSensor;

    ParoSci_202BG_Calibration _calibrator;

    nidas::core::CalFile* _calfile;

    /** No copying. */
    ParoSci_202BG_P(const ParoSci_202BG_P&);

    /** No assignment. */
    ParoSci_202BG_P& operator=(const ParoSci_202BG_P&);

};

}}	// namespace nidas namespace dynld

#endif
