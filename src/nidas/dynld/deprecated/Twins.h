// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_DYNLD_TWINS_H
#define NIDAS_DYNLD_TWINS_H

#include <nidas/core/DSMSensor.h>
#include <nidas/dynld/DSC_A2DSensor.h>

#include <nidas/linux/diamond/dmd_mmat.h>
// #include <nidas/linux/filters/short_filters.h>

#include <vector>
#include <map>
#include <set>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * One or more sensors connected to a Diamond Systems Corp A2D.
 */
class Twins : public DSC_A2DSensor {

public:

    Twins();
    ~Twins();

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    /*
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);

private:

    /**
     * Counter of number of raw samples of wrong size.
     */
    size_t _badRawSamples;

    /**
     * Create the D2A waveform
     */
    void createRamp(const struct DMMAT_D2A_Conversion& conv,D2A_WaveformWrapper& wave);

    /**
     * Size of output waveforms, which is also the size of the
     * output samples.
     */
    int _waveSize;

    /**
     * Waveform output rate in Hz: waveforms/sec, also the rate of the output samples.
     */
    float _waveRate;

    /**
     * Channel number, from 0, of output A2D channel.
     */
    int _outputChannel;

};

}}	// namespace nidas namespace dynld

#endif
