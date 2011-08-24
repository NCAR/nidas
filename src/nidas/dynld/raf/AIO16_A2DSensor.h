/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_RAF_AIO16_A2DSENSOR_H
#define NIDAS_DYNLD_RAF_AIO16_A2DSENSOR_H

#include <nidas/core/DSMSensor.h>

#include <vector>
#include <map>
#include <set>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * A sensor connected to the DSM A2D
 */
class AIO16_A2DSensor : public DSMSensor {

public:

    AIO16_A2DSensor();
    ~AIO16_A2DSensor();

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException
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

    /**
     * Set desired latency, providing some control
     * over the response time vs buffer efficiency tradeoff.
     * Setting a latency of 1/10 sec means buffer
     * data in the driver for a 1/10 sec, then send the data
     * to user space. As implemented here, it must be
     * set before doing a sensor open().
     * @param val Latency, in seconds.
     */
    void setLatency(float val) throw(nidas::util::InvalidParameterException)
    {
        latency = val;
    }

    float getLatency() const { return latency; }

protected:

    bool initialized;

    /* What we need to know about a channel */
    struct chan_info {
        int rate;
	int gain;
	bool bipolar;
    };
    std::vector<struct chan_info> channels;

    /**
     * Correction factors for converting from nominal volts to corrected voltages.
     */
    std::vector<float> corSlopes;

    /**
     * Correction offsets for converting from nominal volts to corrected voltages.
     */
    std::vector<float> corIntercepts;

    /**
     * Requested A2D channels, 0 to (MAXA2DS-1),
     * in the order they were requested.
     */
    std::vector<int> channelNums;

    /*
     * Requested A2D channels, in numeric order.
     */
    std::set<int> sortedChannelNums;

    /**
     * Sample rate of each SampleTag.
     */
    std::vector<int> rateVec;

    /**
     * For each SampleTag, the number of variables.
     */
    std::vector<int> numVarsInSample;

    /**
     * For each SampleTag, its sample id
     */
    std::vector<dsm_sample_id_t> sampleIds;

    /**
     * For each requested variable, which SampleTag does it correspond to,
     * 0:(number_of_samples-1).
     *	
     */
    std::vector<int> sampleIndexVec;

    /**
     * Same info as sampleIndexVec, but in channel order.
     */
    int* sampleIndices;		// optimized version

    /**
     * For each requested variable, which variable within the SampleTag
     * does it correspond to, 0:(num_vars_in_sample - 1)
     */
    std::vector<int> subSampleIndexVec;

    /**
     * Same info as subSampleIndexVec, but in channel order.
     */
    int* subSampleIndices;	// optimized version


    /**
     * Conversion factor when converting from A2D counts to 
     * voltage.  The gain is accounted for in this conversion, so that
     * the resultant voltage is the true input voltage, before
     * any A2D gain was applied.  These are in channel order.
     */
    float *convSlope;

    /**
     * Conversion offset when converting from A2D counts to 
     * voltage.  The polarity is accounted for in this conversion, so that
     * the resultant voltage is the true input voltage.
     * These are in channel order.
     */
    float *convIntercept;

    /**
     * For each SampleTag, the next sample timetag to be output.
     * This value is incremented by the sample deltat
     * (1/rate) after each result sample is output.
     */
    dsm_time_t *sampleTimes;

    /**
     * The output delta t, 1/rate, in microseconds.
     */
    int *deltatUsec;

    /*
     * Allocated samples.
     */
    SampleT<float>** outsamples;

    /**
     * Sensor latency, in seconds.
     */
    float latency;

    /**
     * Counter of number of raw samples of wrong size.
     */
    size_t badRawSamples;

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
