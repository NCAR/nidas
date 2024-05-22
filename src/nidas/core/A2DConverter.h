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
 * The readCalFile method reads an A2D calibration file,
 * containing the time of each calibration and the calculated
 * conversion coefficients to be applied for the channel gain
 * and polarity that were used in the calibration.
 *
 * Data members in this class include the gain and
 * bi-polarity of each channel to be converted.  The gain
 * and polarity are used when selecting records from the
 * calibration file.
 */
class A2DConverter {

public:

    A2DConverter(int nchan, int ncoefs);

    virtual ~A2DConverter();

    int getMaxNumChannels() const { return _maxNumChannels; }

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

    virtual void fillNAN() = 0;

    /**
     * Read records from a CalFile for calibration coefficients
     * with time tags less than or equal to tt, assuming they
     * are in increasingg order in the file. Each record
     * has a gain and bipolar value after the time, followed
     * by coefficients for each channel. The A2DConverter
     * for a channel with a matching gain and bipolar value are updated.
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

    void fillNAN();

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

    void fillNAN();
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
