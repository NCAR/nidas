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

    DSMAnalogSensor();
    ~DSMAnalogSensor();

    bool isRTLinux() const;

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner();

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    void init() throw(nidas::util::InvalidParameterException);
                                                                                
    /*
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Process a raw sample, which in this case means unpack the
     * A2D data buffer into individual samples and convert the
     * counts to voltage.
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

    void addSampleTag(SampleTag* tag)
            throw(nidas::util::InvalidParameterException);

    int gainSetting(float gain)
	    throw(nidas::util::InvalidParameterException);

    /**
     * Get the current temperature. Sends a ioctl to the driver module.
     */
    float getTemp() throw(nidas::util::IOException);

    void readCalFile(dsm_time_t tt) throw(nidas::util::IOException);

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
    bool initialized;

    /* What we need to know about a channel */
    struct chan_info {
	int gainSetting;
	int gainMul;
	int gainDiv;
        int index;      // which sample does this channel go to
	bool bipolar;
        bool rawCounts;
    };
    std::vector<struct chan_info> _channels;

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

    /**
     * Conversion factor from 16 bit raw temperature to degC
     */
    const float DEGC_PER_CNT;

    dsm_time_t _calTime;
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
