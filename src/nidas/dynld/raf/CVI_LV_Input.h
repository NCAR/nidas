// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_RAF_CVI_LV_INPUT_H
#define NIDAS_DYNLD_RAF_CVI_LV_INPUT_H

#include <nidas/core/CharacterSensor.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Support for reading the output from the LabView process on the CVI PC.
 * Adjust the time tag on the samples that come back from LabView
 * by overriding the CharacterSensor::process() method.
 */
class CVI_LV_Input: public CharacterSensor
{

public:

    CVI_LV_Input();

    void open(int flags)
	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    IODevice* buildIODevice() throw(nidas::util::IOException);

    /**
     * Adjust the processing of samples which come back from LabView.
     * The first field in the received sample is a seconds-of-day
     * value that was sent to LabView, indicating when the raw data
     * for the sample was sampled.  LabView then echoes the value
     * back, along with some of the raw data and some derived variables.
     * We use the seconds-of-day value to adjust the timetag on the
     * processed data to match when the raw data was sampled, rather
     * than the time the sample was received from LabView.
     */
    bool process(const Sample * samp,list < const Sample * >&results) throw();

private:

    /**
     * Time at 00:00:00 UTC of day
     */
    dsm_time_t _tt0;

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
