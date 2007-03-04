/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_RAF_DSC_A2DSENSOR_H
#define NIDAS_DYNLD_RAF_DSC_A2DSENSOR_H

#include <nidas/core/DSMSensor.h>

#include <nidas/linux/filters/short_filters.h>

#include <vector>
#include <map>
#include <set>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * A sensor connected to the DSM A2D
 */
class DSC_A2DSensor : public DSMSensor {

public:

    DSC_A2DSensor();
    ~DSC_A2DSensor();

    bool isRTLinux() const;

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

    /**
     * Set desired latency, providing some control
     * over the response time vs buffer efficiency tradeoff.
     * Setting a latency of 1/10 sec means buffer
     * data in the driver for a 1/10 sec, then send the data
     * to user space. As implemented here, it must be
     * set before doing a sensor open().
     * @param val Latency, in seconds.
     */
    // void setLatency(float val) throw(nidas::util::InvalidParameterException)
    // {
      //   latency = val;
    // }

    // float getLatency() const { return latency; }

    void setScanRate(int val) { scanRate = val; }

    int getScanRate() const { return scanRate; }

    void fromDOMElement(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);

private:

    bool initialized;

    int scanRate;     // requested sample rate

    /* What we need to know about a channel */
    struct chan_info {
	int gain;   // 0 means this channel is not sampled
	bool bipolar;
        int id;     // which sample id does this channel belong to
    };
    std::vector<struct chan_info> channels;

    struct sample_info {
        /**
         * Full sample id
         */
        dsm_sample_id_t sampleId;

        /**
         * "little" sample id, 0,1,2 etc
         */
        int id;

        /**
         * Output rate of this sample
         */
        int rate;                       // output rate

        /**
         * Which filter is being applied to these samples.
         */
        enum nidas_short_filter filterType;

        /**
         * Number of points in boxcar averages, if boxcar filter.
         */
        int boxcarNpts;

        /** 
         * Number of variables in the sample.
         */
        unsigned int nvars;

        /**
         * Conversion factor when converting from A2D counts to 
         * voltage.  The gain is accounted for in this conversion, so that
         * the resultant voltage is the true input voltage, before
         * any A2D gain was applied.  These are in the order of
         * the variables in the sample.
         */
        float* convSlopes;
        /**
         * Conversion offset when converting from A2D counts to 
         * voltage.  The polarity is accounted for in this conversion, so that
         * the resultant voltage is the true input voltage.
         * These are in channel order.
         */
        float* convIntercepts;
    };
    std::vector<struct sample_info> samples;
      
    /**
     * Sensor latency, in seconds.
     */
    // float latency;

    /**
     * Counter of number of raw samples of wrong size.
     */
    size_t badRawSamples;

    mutable int rtlinux;
};

}}	// namespace nidas namespace dynld

#endif
