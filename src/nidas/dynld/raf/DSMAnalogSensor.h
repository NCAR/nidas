/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_RAF_DSMANALOGSENSOR_H
#define NIDAS_DYNLD_RAF_DSMANALOGSENSOR_H

#include <nidas/dynld/A2DSensor.h>
#include <nidas/linux/ncar_a2d.h>
#include <nidas/linux/irigclock.h>

#include <vector>
#include <map>
#include <set>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * A sensor connected to the DSM A2D
 */
class DSMAnalogSensor : public A2DSensor {

public:
    enum OutputMode { Counts, Volts, Engineering };

    DSMAnalogSensor();
    ~DSMAnalogSensor();

    bool isRTLinux() const;

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    /*
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

    void printStatus(std::ostream& ostr) throw();

    int getMaxNumChannels() const { return NUM_NCAR_A2D_CHANNELS; }

    void setA2DParameters(int ichan,int gain,int bipolar)
               throw(nidas::util::InvalidParameterException);

    void getBasicConversion(int ichan,float& intercept, float& slope) const;

    void setConversionCorrection(int ichan, float corIntercept, float corSlope)
	throw(nidas::util::InvalidParameterException);


    void setOutputMode(OutputMode mode) { _outputMode = mode; }

    OutputMode getOutputMode() const { return _outputMode; }

    /**
     * Process a raw sample, which in this case means unpack the
     * A2D data buffer into individual samples and convert the
     * counts to voltage.
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

    void addSampleTag(SampleTag* tag)
            throw(nidas::util::InvalidParameterException);

    /**
     * Get the current temperature. Sends a ioctl to the driver module.
     */
    float getTemp() throw(nidas::util::IOException);

    void readCalFile(dsm_time_t tt) throw(nidas::util::IOException);

    int getInt32TimeTagUsecs() const 
    {
        return USECS_PER_MSEC;
    }

protected:

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

    mutable int rtlinux;

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

    dsm_time_t _calTime;

    /**
     * Whethere to output samples as counts, volts or engineering units.  Decides
     * which calibrations to apply.
     * @see enum OutputMode
     */
    OutputMode _outputMode;

    /**
     * Conversion factor from 16 bit raw temperature to degC
     */
    static const float DEGC_PER_CNT = 0.0625;
//@{
    /**
     * Given a measured voltage and using the A/D temperature, perform a lookup
     * using two dimentional interpolation to get the actual voltage.
     * @see _currentTemperature
     * @param measured voltage.
     * @returns actual voltage.
     */
    float voltageActual(float voltageMeasured);

    /**
     * Capture the current A/D board temperature, so we can do a temperature
     * drift compensation to all the raw analog counts/voltages.
     */
    float _currentTemperature;

    /**
     * How many different Voltages were measured at each temperature in the chamber.
     * Every 1 Vdc from -10 to 10.
     */
    static const int N_VOLTAGES = 21;

    /**
     * How many different temperature stages were measured in the chamber.
     * Every 10 degrees from 10C to 60C.
     */
    static const int N_DEG = 6;

    /**
     * Temperature Chamber was done increments of 10C (i.e. 10C, 20C, 30C, etc).
     * The values in this area are what the A/D card was measuring (which is about
     * 2C warmer than the chamber).
     */
    static const float TemperatureChamberTemperatures[N_DEG];

    /**
     * Table for -10 to 10 Volts.
     */
    static const float TemperatureTableGain1[N_VOLTAGES][N_DEG];
//@}
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
