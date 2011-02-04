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

#include <nidas/linux/a2d.h>
// #include <nidas/linux/filters/short_filters.h>

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

    void setScanRate(int val) { _scanRate = val; }

    int getScanRate() const { return _scanRate; }

    bool process(const Sample* insamp,std::list<const Sample*>& results) throw();

    void fromDOMElement(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);


    /**
     * Return the maximum possible number of A2D channels
     * on this device.  Derived classes must implement
     * this method.
     */
    virtual int getMaxNumChannels() const = 0;

    /**
     * Set the sampling gain and polarity for a channel.
     * Derived classes are just responsible for throwing
     * nidas::util::InvalidParameterException in case of bad
     * values. If the values are OK, they should call base class
     * A2DSensor::setA2DParameters().
     * @param bipolar: 0=unipolar, 1=bipolar
     */
    virtual void setA2DParameters(int ichan,int gain,int bipolar)
        throw(nidas::util::InvalidParameterException);

    /**
     * Get the current gain and bipolar parameters for a channel.
     */
    virtual void getA2DParameters(int ichan,int& gain,int& bipolar) const;

    /**
     * Get the current gain for a channel.
     */
    int getGain(int ichan) const;

    /**
     * Get the current bipolar parameter for a channel.
     * @return 1: bipolar, 0: unipolar, -1: unknown
     */
    int getBipolar(int ichan) const;

    /**
     * Return the current linear conversion for a channel. The
     * A2D parameters of gain and bipolar should have already
     * been set. If they have not been set, derived classes should
     * return FloatNAN for those values.
     */
    virtual void getBasicConversion(int ichan,float& intercept, float& slope) const = 0;

    /**
     * Set the values for a linear correction to the basic conversion.
     * An intercept of 0. and a slope of 1. would result in no
     * additional correction.
     */
    virtual void setConversionCorrection(int ichan,float intercept,
        float slope) throw(nidas::util::InvalidParameterException);


    /**
     * Get the values for a linear correction to the basic conversion.
     */
    void getConversion(int ichan,float& intercept, float& slope) const;

    /**
     * Get the current conversion slope, which includes any 
     * correction as set by setConversionCorrection().
     */
    float getSlope(int ichan) const
    {
        if (ichan < 0 || ichan >= _maxNChannels) return floatNAN;
        return _convSlopes[ichan];
    }

    /**
     * Get the current conversion intercept, which includes any 
     * correction as set by setConversionCorrection().
     */
    float getIntercept(int ichan) const
    {
        if (ichan < 0 || ichan >= _maxNChannels) return floatNAN;
        return _convIntercepts[ichan];
    }

protected:

    class A2DSampleInfo
    {
    public:
        A2DSampleInfo(int n)
            : nvars(n),nvalues(0),stag(0),channels(new int[nvars]) {}
        ~A2DSampleInfo() { delete [] channels; }
        int nvars;
        int nvalues;
        const SampleTag* stag;
        int *channels;
    private:
        A2DSampleInfo(const A2DSampleInfo& x);
        A2DSampleInfo& operator= (const A2DSampleInfo& x);
    };

    class A2DSampleConfig: public nidas_a2d_sample_config
    {
    public:
        virtual ~A2DSampleConfig() {}
    };

    class A2DBoxcarConfig: public A2DSampleConfig
    {
    public:
        A2DBoxcarConfig(int n): npts(n)
        {
            assert((void*)&filterData[0] == (void*)&npts);
        }
        int npts;
    };

    std::vector<struct A2DSampleConfig*> _sampleCfgs;

    std::vector<A2DSampleInfo*> _sampleInfos;

    /**
     * Counter of number of raw samples of wrong size.
     */
    size_t _badRawSamples;

protected:
    void initParameters();

    int _maxNChannels;

    /**
     * Conversion factor for each channel when converting from A2D
     * counts to voltage.
     * The gain is accounted for in this conversion, so that
     * the resultant voltage value is an estimate of the actual
     * input voltage, before any A2D gain was applied.
     */
    float* _convSlopes;

    /**
     * Conversion offset for each A2D channel when converting from A2D
     * counts to voltage.
     * The polarity is accounted for in this conversion, so that
     * the resultant voltage value should be the actual input voltage.
     */
    float* _convIntercepts;

private:
    /**
     * Requested A2D sample rate before decimation.
     */
    int _scanRate;

    int _prevChan;

    int* _gains;

    int* _bipolars;
};

}}	// namespace nidas namespace dynld

#endif
