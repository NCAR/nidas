/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-04-22 10:12:41 -0600 (Sun, 22 Apr 2007) $

    $LastChangedRevision: 3836 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/DSC_A2DSensor.h $

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_A2DSENSOR_H
#define NIDAS_DYNLD_A2DSENSOR_H

#include <nidas/core/DSMSensor.h>

#include <nidas/linux/filters/short_filters.h>

#include <vector>
#include <map>
#include <set>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * One or more sensors connected to an A2D
 */
class A2DSensor : public DSMSensor {

public:

    A2DSensor();
    ~A2DSensor();

    bool isRTLinux() const;

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

    void addSampleTag(SampleTag* tag)
            throw(nidas::util::InvalidParameterException);

    void setScanRate(int val) { scanRate = val; }

    int getScanRate() const { return scanRate; }

    bool process(const Sample* insamp,std::list<const Sample*>& results) throw();

    void fromDOMElement(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);

protected:

    void config() throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    bool initialized;

    /**
     * Requested A2D sample rate before decimation.
     */
    int scanRate;

    /* What we need to know about a channel */
    struct chan_info {
	int gain;   // 0 means this channel is not sampled
	bool bipolar;
        int index;     // index of sample for this channel
    };
    std::vector<struct chan_info> _channels;

    struct sample_info {
        /**
         * The tag of each requested sample.
         */
        SampleTag* stag;

        /**
         * A2D sample index, 0,1,2 etc
         */
        int index;

        /**
         * Output rate of this sample, after decimation.
         */
        int rate;

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
         * voltage for each variable in the sample.
         * The gain is accounted for in this conversion, so that
         * the resultant voltage value is an estimate of the actual
         * input voltage, before any A2D gain was applied.
         */
        float* convSlopes;

        /**
         * Conversion offset when converting from A2D counts to 
         * voltage.  The polarity is accounted for in this conversion, so that
         * the resultant voltage value should be the actual input voltage.
         */
        float* convIntercepts;
    };
    std::vector<struct sample_info> _samples;
      
    /**
     * Counter of number of raw samples of wrong size.
     */
    size_t badRawSamples;

    mutable int rtlinux;
};

}}	// namespace nidas namespace dynld

#endif
