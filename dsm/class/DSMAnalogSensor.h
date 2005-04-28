/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef DSMANALOGSENSOR_H
#define DSMANALOGSENSOR_H

#include <RTL_DSMSensor.h>

#include <vector>
#include <map>

namespace dsm {
/**
 * A sensor connected to the DSM A2D
 */
class DSMAnalogSensor : public RTL_DSMSensor {

public:

    DSMAnalogSensor();
    ~DSMAnalogSensor();

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(atdUtil::IOException);

    void init() throw();
                                                                                
    /*
     * Close the device connected to the sensor.
     */
    void close() throw(atdUtil::IOException);

    /**
     * Process a raw sample, which in this case means do
     * a sscanf on the character string contents, creating
     * a processed sample of binary floating point data.
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

    void addSampleTag(SampleTag* tag)
            throw(atdUtil::InvalidParameterException);

    /**
     * Synchronize the A/D's with 1PPS from IRIG/GPS
     */
    void run(int msg) throw(atdUtil::IOException);

    int DSMAnalogSensor::rateSetting(float rate)
	    throw(atdUtil::InvalidParameterException);

    int DSMAnalogSensor::gainSetting(float gain)
	    throw(atdUtil::InvalidParameterException);

protected:

    bool initialized;

    /* What we need to know about a channel */
    struct chan_info {
        float rate;
	int rateSetting;
	float gain;
	int gainSetting;
	bool bipolar;
    };
    std::vector<struct chan_info> channels;

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
    int* sampleIndices;		// optimized version

    /**
     * For each requested variable, which variable within the SampleTag
     * does it correspond to, 0:(num_vars_in_sample - 1)
     */
    std::vector<int> subSampleIndexVec;
    int* subSampleIndices;	// optimized version


    /**
     * Conversion factor when converting from A2D counts to 
     * voltage.  The gain is accounted for in this conversion, so that
     * the resultant voltage is the true input voltage, before
     * any A2D gain was applied.
     */
    float *convSlope;

    /**
     * Conversion offset when converting from A2D counts to 
     * voltage.  The polarity is accounted for in this conversion, so that
     * the resultant voltage is the true input voltage.
     */
    float *convIntercept;

    /**
     * For each SampleTag, the end time of the current sample
     * window. This value is incremented by the sample deltat
     * (1/rate) after each result sample is output.
     */
    dsm_time_t *endTimes;

    /**
     * If the delta t is not an even number of milliseconds,
     * then we calculate the endTimes by
     * @code
     *	endTimes[i] = baseTimes[i] + deltatDouble[i] * ++nsamples[i];
     * @endcode
     * in order to avoid accumulation of errors.
     */
    dsm_time_t *baseTimes;

    /**
     * Number of samples output for each SampleTag.
     */
    size_t *nsamps;

    /**
     * The output delta t, 1/rate, in milliseconds.
     */
    double *deltatDouble;

    /**
     * The output delta t, 1/rate, in milliseconds.
     */
    int *deltatInt;

    /**
     * Whether we can use deltatInt.
     */
    bool *deltatEvenMsec;

    double minDeltatDouble;

    int minDeltatInt;

    bool minDeltatEvenMsec;

    unsigned int nSamplePerRawSample;

    /*
     * Allocated samples.
     */
    SampleT<float>** outsamples;

    const float floatNAN;
};

}

#endif
