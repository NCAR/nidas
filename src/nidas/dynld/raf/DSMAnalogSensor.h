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

#include <nidas/core/DSMSensor.h>

#include <vector>
#include <map>
#include <set>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * A sensor connected to the DSM A2D
 */
class DSMAnalogSensor : public DSMSensor {

public:

    DSMAnalogSensor();
    ~DSMAnalogSensor();

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner();

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException);

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

    int rateSetting(float rate)
	    throw(nidas::util::InvalidParameterException);

    int gainSetting(float gain)
	    throw(nidas::util::InvalidParameterException);

protected:

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
        float rate;
	int rateSetting;
	float gain;
	int gainSetting;
	int gainMul;
	int gainDiv;
	bool bipolar;
        bool rawCounts;
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
    std::vector<float> rateVec;

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

    int minDeltatUsec;

    unsigned int nSamplePerRawSample;

    /*
     * Allocated samples.
     */
    SampleT<float>** outsamples;

    /**
     * Expected size of raw sample.
     */
    size_t rawSampleLen;

    /**
     * Counter of number of raw samples of wrong size.
     */
    size_t badRawSamples;
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
