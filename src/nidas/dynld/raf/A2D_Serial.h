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

#ifndef _nidas_dynld_raf_a2d_serial_h_
#define _nidas_dynld_raf_a2d_serial_h_

#include <nidas/core/SerialSensor.h>

#include <nidas/util/InvalidParameterException.h>


// change this later to a const or something
#define NUM_A2D_CHANNELS    4


namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * A2D Serial Sensor.  This would be able to use the generic SerialSensor class
 * except for the need to manfacture time-stamps.  Data is sampled in the A2D
 * at exact intervals, but serial time-stamping is mediocre.  We want no time-lagging
 * downstream that might affect spectral characteristics.
 */
class A2D_Serial : public SerialSensor
{

public:
    enum OutputMode { Counts, Volts, Engineering };

    A2D_Serial();
    ~A2D_Serial();

    /**
     * open the sensor and perform any intialization to the driver.
     */
    void open(int flags) throw(nidas::util::IOException);

    /**
     * Setup whatever is necessary for process method to work.
     */
    void init() throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

    void validate() throw(nidas::util::InvalidParameterException);

    int getMaxNumChannels() const { return NUM_A2D_CHANNELS; }

    /**
     * Get the current gain for a channel.
     */
    int getGain(int ichan) const;

    /**
     * Get the current bipolar parameter for a channel.
     * @return 1: bipolar, 0: unipolar, -1: unknown
     */
    int getBipolar(int ichan) const;

    /**
     * Set the values for a linear correction.  An intercept of 0.
     * and a slope of 1. would result in no additional correction.
     */
    virtual void setConversionCorrection(int ichan, const float d[],
        int n) throw(nidas::util::InvalidParameterException);

    void setOutputMode(OutputMode mode) { _outputMode = mode; }

    OutputMode getOutputMode() const { return _outputMode; }


protected:
    /**
     * Read configuration from sensor.
     */
    void readConfig() throw(nidas::util::IOException);

    /**
     * Check the checksum for data lines.  Header and config lines have no
     * checksum.
     */
    bool checkCkSum(const Sample *samp, const char *data);


    /**
     * Read calibration file for this A2D. Does not throw exceptions,
     * since it is used in the process method, but instead logs errors.
     */
    void readCalFile(dsm_time_t tt) throw();


    /**
     * Number of variables to decode.
     */
    int _nVars;

    size_t _sampleRate;
    size_t _deltaT;

    /**
     * CalFile for this A2D_Serial sensor.  This is for the A2D cals, not
     * engineering cals.
     */
    CalFile *_calFile;

    /**
     * Whethere to output samples as counts, volts or engineering units.  Decides
     * which calibrations to apply.
     * @see enum OutputMode
     */
    OutputMode _outputMode;

    /**
     * Is device receiving PPS.  We read it from header packet.
     */
    size_t _havePPS;

    int _gains[NUM_A2D_CHANNELS];
    int _bipolars[NUM_A2D_CHANNELS];
    std::vector<float> _polyCals[NUM_A2D_CHANNELS];

    size_t _shortPacketCnt;
    size_t _badCkSumCnt;
    size_t _largeTimeStampOffset;

int headerLines;

private:

    /** No copying. */
    A2D_Serial(const A2D_Serial&);

    /** No assignment. */
    A2D_Serial& operator=(const A2D_Serial&);

};

}}}                     // namespace nidas namespace dynld namespace raf

#endif
