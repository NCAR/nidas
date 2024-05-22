// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_DYNLD_RAF_DSMANALOGSENSOR_H
#define NIDAS_DYNLD_RAF_DSMANALOGSENSOR_H

#include <nidas/dynld/A2DSensor.h>
#include <nidas/core/A2DConverter.h>
#include <nidas/linux/ncar_a2d.h>
#include <nidas/linux/irigclock.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Support for the PC104 A2D, developed at NCAR EOL.
 */
class DSMAnalogSensor : public A2DSensor {

public:

    DSMAnalogSensor();
    ~DSMAnalogSensor();

    /**
     * @throws nidas::util::IOException
     **/
    IODevice* buildIODevice();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    SampleScanner* buildSampleScanner();

    /**
     * Open the device connected to the sensor.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void open(int flags);

    /**
     * Close the device connected to the sensor.
     *
     * @throws nidas::util::IOException
     **/
    void close();

    /**
     * Called prior to any call to process().
     *
     * @throws nidas::util::InvalidParameterException
     **/
    void init();

    void printStatus(std::ostream& ostr) throw();

    int getMaxNumChannels() const { return NUM_NCAR_A2D_CHANNELS; }

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void setGainBipolar(int ichan, int gain, int bipolar);

    void getDefaultConversion(int ichan, float& intercept, float& slope) const;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void setConversionCorrection(int ichan, float corIntercept, float corSlope);

    /**
     * Process a raw sample, which in this case means unpack the
     * A2D data buffer into individual samples and convert the
     * counts to voltage.
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void validate();

    /**
     * Get the current temperature. Sends a ioctl to the driver module.
     *
     * @throws nidas::util::IOException
     **/
    float getTemp();

    int getInt32TimeTagUsecs() const
    {
        return USECS_PER_MSEC;
    }

    void executeXmlRpc(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw();

    void getA2DSetup(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw();

    void testVoltage(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw();

    /**
     * Initial A2DConverter, containing the default conversion from
     * counts to volts based on the gain and bipolar settings of each channel.
     */
    A2DConverter* getInitialConverter() const
    {
        return _initialConverter;
    }

    /**
     * Final A2DConverter, updated from the CalFile, applied after the initial
     * conversion, and an optional temperature compensation.
     * Since all existing cal files have two coefficients
     * for each channel, it must be a LinearA2DConverter.
     */
    A2DConverter* getFinalConverter() const
    {
        return _finalConverter;
    }

protected:

    A2DConverter* _initialConverter;

    A2DConverter* _finalConverter;

    bool processTemperature(const Sample*, std::list<const Sample*>& result) throw();

    /**
     * Read a filter file containing coefficients for an Analog Devices
     * A2D chip.  Lines that start with a '#' are skipped.
     * @param name Name of file to open.
     * @param coefs Pointer to coefficient array.
     * @param nexpect Number of expected coefficients. readFilterFile
     *		will throw an IOException if it reads more or less
     *		coeficients than nexpect.
     */
    int readFilterFile(const std::string& name,unsigned short* coefs,
	int nexpect);

    /**
     * The output delta t, 1/rate, in microseconds.
     */
    int _deltatUsec;

    /**
     * Pointer to temperature SampleTag if user has
     * asked for it, otherwise NULL.
     */
    const SampleTag* _temperatureTag;

    /**
     * Rate of requested A2D board temperature,
     * as an IRIG enumerated rate.
     */
    enum irigClockRates _temperatureRate;

    /**
     * Conversion factor from 16 bit raw temperature to degC
     */
    static const float DEGC_PER_CNT;
//@{
    /**
     * Given a measured voltage and using the A/D temperature, perform a lookup
     * using two dimentional interpolation to get the actual voltage.
     * @see _currentTemperature
     * @param measured voltage.
     * @returns actual voltage.
     */
    float voltageActual(float voltageMeasured);

    inline float SecondPoly(float x, const float c[]) const
	{ return(c[0] + x * (c[1] + x * c[2])); }

    /**
     * Capture the current A/D board temperature, so we can do a temperature
     * drift compensation to all the raw analog counts/voltages.
     */
    float _currentTemperature;

    /**
     * How many different Voltages were measured at each temperature in the chamber.
     * Every 1 Vdc from -10 to 10.
     */
    static const int N_COEFF = 3;

    /**
     * How many different temperature stages were measured in the chamber.
     * Every 10 degrees from 10C to 60C.
     */
    static const int N_G4_VDC = 9;

    /**
     * Temperature Chamber for 0-5 volts was done every half volt.  This is
     * the list of voltages.
     */
    static const float TemperatureChamberVoltagesGain4[N_G4_VDC];

    /**
     * Table of coefficients for 0 to 5 Volts.
     */
    static const float TemperatureTableGain4[N_G4_VDC][N_COEFF];
//@}

private:

    /** No copying. */
    DSMAnalogSensor(const DSMAnalogSensor&);

    /** No assignment. */
    DSMAnalogSensor& operator=(const DSMAnalogSensor&);

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
