// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_CORE_A2DCONVERTER_H
#define NIDAS_CORE_A2DCONVERTER_H

#include "Sample.h"

namespace nidas { namespace core {

class CalFile;

/**
 * Virtual class with methods for performing conversions from
 * integer A2D counts to floating point voltages for each
 * channel of an A2D.
 *
 * Two subclasses are defined, LinearA2DConverter and PolyA2DConverter.
 * A2D sensor classes instantiate whichever type they want to use.
 *
 * The gain and bipolar value for each channel in the A2DConverter are
 * set by the A2D sensor, typically at configuration time in the
 * validate() method.  A gain of 0 indicates that a channel is not used.
 *
 * Our A2Ds (DSMAnalogSensor, DSC_A2DSensor, A2D_Serial) typically
 * instantiate two converters. A LinearA2DConverter is used to convert
 * counts to approximate voltages using the known voltage transformations
 * of the A2D, at a given gain and bipolar value for each channel.
 * 
 * A second converter, often a 3rd order PolyA2DConverter, then
 * applies corrections to obtain a calibrated voltage.

 * If an A2D CalFile has been configured for the sensor, the readCalFile()
 * method of the second converter can be called to read the conversion
 * coefficients from the file.  See the doc for the readCalFile()
 * method for information about the number of expected values
 * in the calibration file.
 */
class A2DConverter {

public:

    A2DConverter(int nchan, int ncoefs);

    virtual ~A2DConverter();

    /**
     * Maximum possible number of channels.
     */
    int getMaxNumChannels() const { return _maxNumChannels; }

    /**
     * One plus the index of the last active channel.
     * An active channel has a gain > 0.
     */
    int getNumConfigChannels() const { return _numConfigChannels; }

    /**
     * Gain of each channel.  An A2DConverter needs to know
     * the gain and bipolarity because it reads the cal file
     * and there are typically cal records for each gain and polarity.
     */
    int getGain(int ichan) const;

    void setGain(int ichan, int val);

    int getBipolar(int ichan) const;

    void setBipolar(int ichan, int val);

    /**
     * Convert  a count to a floating point value.
     */
    virtual float convert(int ichan, float counts) const = 0;

    /**
     * Set the initial conversion for a channel
     */
    virtual void set(int ichan, const float* d, int nd) = 0;

    virtual void get(int ichan, float* d, int nd) const = 0;

    virtual void setNAN(int ichan) = 0;

    virtual void setNAN() = 0;

    /**
     * Read records from a CalFile for calibration coefficients
     * with time tags less than or equal to tt, assuming they
     * are in increasingg order in the file. Each record
     * has a gain and bipolar value after the time, followed
     * by coefficients for each channel.
     *
     * A calibration record is used for a channel if its
     * gain and bipolar fields match the values for the configured
     * channel. A gain value of -1 in the file is a wildcard, matching
     * any configured gain for a channel, and likewise for the bipolar value.
     *
     * readCalFile() expects there to be
     *      getNumConfigChannels() X _ncoefs
     * number of coefficients after the time, gain and bipolar fields in
     * each CalFile record.  If there are less than the expected
     * number, a warning is logged, and missing coefficients are
     * set to floatNAN.
     *
     * @throws: EOFException, IOException, ParseException
     */
    void readCalFile(CalFile* cf, dsm_time_t tt);

protected:

    /**
     * How many channels on the A2D. Set in constructor.
     */
    int _maxNumChannels;

    /**
     * A configured channel has a gain > 0.
     * The number of configured channels is the index of the last
     * configured channel plus one.
     */
    int _numConfigChannels;

    /**
     * Number of coefficients in the conversion.
     */
    int _ncoefs;

    /**
     * Gain setting of each channel. 0 for an unused channel.
     */
    int *_gain;

    /**
     * Polarity setting, 1=bi-polar. e.g -5 to 5V, 0=unipolar, e.g. 0 to 5V,
     * -1 for an unused channel.
     */
    int *_bipolar;

private:
    /**
     * No copy
     */
    A2DConverter(const A2DConverter& x);

    /** No assign */
    A2DConverter& operator=(const A2DConverter& x);
};

/**
 * A2DConverter for applying a linear conversion to A2D counts values. 
 */
class LinearA2DConverter : public A2DConverter {

public:
    LinearA2DConverter(int nchan);

    ~LinearA2DConverter();

    float convert(int ichan, float counts) const;

    /**
     * Set the initial linear conversion for a channel.
     */
    void set(int ichan, const float* d, int nd);

    void get(int ichan, float* d, int nd) const;

    void setNAN(int ichan);

    void setNAN();

private:

    /**
     * Conversion intercepts for each channel.
     */
    float *_b;

    /**
     * Conversion slopes for each channel.
     */
    float *_mx;

    /**
     * No copy
     */
    LinearA2DConverter(const LinearA2DConverter& x);

    /** No assign */
    LinearA2DConverter& operator=(const LinearA2DConverter& x);
};

/**
 * A2DConverter for applying a polynomial conversion to A2D counts values. 
 */
class PolyA2DConverter : public A2DConverter {

public:
    PolyA2DConverter(int nchan, int ncoefs);

    ~PolyA2DConverter();

    float convert(int ichan, float counts) const;

    /**
     * Set the initial polynomial conversion for a channel.
     */
    void set(int ichan, const float* d, int nd);

    void get(int ichan, float* d, int nd) const;

    void setNAN(int ichan);

    void setNAN();
private:

    float** _d;

    /**
     * No copy.
     */
    PolyA2DConverter(const PolyA2DConverter& x);

    /** No assign. */
    PolyA2DConverter& operator=(const PolyA2DConverter& x);
};
}}	// namespace nidas namespace core

#endif
