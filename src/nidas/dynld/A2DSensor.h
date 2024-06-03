// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_DYNLD_A2DSENSOR_H
#define NIDAS_DYNLD_A2DSENSOR_H

#include <nidas/core/DSMSensor.h>
#include <nidas/core/A2DConverter.h>

#include <nidas/linux/a2d.h>

#include <vector>
#include <map>
#include <set>


namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * Virtual base class for supporting sensors attached to an A2D.
 */
class A2DSensor : public DSMSensor {

public:

    enum OutputMode {Counts, Volts, Engineering};

    /**
     * Sub-class constructors know how many channels are on the A2D.
     */
    A2DSensor(int nchan);

    virtual ~A2DSensor();

    /**
     * Open the device connected to the sensor.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void open(int flags);

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void init();

    /*
     * Close the device connected to the sensor.
     *
     * @throws nidas::util::IOException
     **/
    void close();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void validate();

    void setScanRate(int val) { _scanRate = val; }

    int getScanRate() const { return _scanRate; }

    /**
     * Create processed A2D samples.  As of now the only
     * subclass that uses this base class process() method
     * is the Diamond DSC_A2DSensor.  The NCAR DSMAnalogSensor
     * does specialized temperature compensation.
     */
    bool process(const Sample* insamp,std::list<const Sample*>& results) throw();

    /**
     * Return the number of A2D channels on this device,
     * set in the constructor.
     */
    int getMaxNumChannels() const { return _maxNumChannels; }

    /**
     * Set the gain and bipolar parameters for a channel.
     * Sub-classes can throw InvalidParameterException if
     * the combination is not supported.
     */
    virtual void setGainBipolar(int ichan,int gain,int bipolar);

    /**
     * Get the gain for a channel.
     */
    int getGain(int ichan) const
    {
        if (ichan < 0 || ichan >=  getMaxNumChannels()) return 0;
        return getFinalConverter()->getGain(ichan);
    }

    /**
     * Get the polarity parameter for a channel.
     * @return 1: bipolar, 0: unipolar, -1: unknown
     */
    int getBipolar(int ichan) const
    {
        if (ichan < 0 || ichan >=  getMaxNumChannels()) return -1;
        return getFinalConverter()->getBipolar(ichan);
    }

    /**
     * Initial A2DConverter, which typically contains a
     * default linear conversion from counts to volts based on 
     * the gain and bipolar settings of each channel.
     */
    virtual A2DConverter* getInitialConverter() const = 0;

    /**
     * Final A2DConverter, updated from the CalFile, and
     * applied after the initial conversion.
     * The number of coefficients in the A2DConverter must match
     * the number of coefficients for each channel in each record
     * of the CalFile.
     */
    virtual A2DConverter* getFinalConverter() const = 0;

    /**
     * Get the default linear conversion for a channel.
     */
    virtual void getDefaultConversion(int chan, float& intercept, float& slope) const = 0;

    /**
     * Whether to output samples as counts, volts or engineering units.
     * Decides which calibrations to apply.
     * @see enum OutputMode
     */
    void setOutputMode(OutputMode mode) { _outputMode = mode; }

    OutputMode getOutputMode() const { return _outputMode; }

protected:

    int _maxNumChannels;

    /**
     * CalFile for the final A2DConverter.  This is for the A2D cals, not
     * engineering cals of volts to final scientific units.
     */
    CalFile* _calFile;

    /**
     * @see enum OutputMode
     */
    OutputMode _outputMode;

    /**
     * A2D configuration information that is sent to the A2D device module.
     * This is a C++ wrapper for struct nidas_a2d_sample_config
     * providing a virtual destructor. The filterData in this
     * configuration is empty, with a nFilterData of zero.
     */
    class A2DSampleConfig
    {
    public:
        A2DSampleConfig(): _cfg()  {}
        virtual ~A2DSampleConfig() {}
        virtual nidas_a2d_sample_config& cfg() { return _cfg; }
    private:
        nidas_a2d_sample_config _cfg;
        // No copying or assignment
        A2DSampleConfig(const A2DSampleConfig& x);
        A2DSampleConfig& operator=(const A2DSampleConfig& rhs);
    };

    /**
     * A2D configuration for box-car averaging of A2D samples.
     * filterData[] contains the number of samples in the average.
     */
    class A2DBoxcarConfig: public A2DSampleConfig
    {
    public:
        A2DBoxcarConfig(int n): A2DSampleConfig(),npts(n)
        {
            // make sure there is no padding or extra bytes
            // between the end of nidas_a2d_sample_config and npts.
            // The driver C code will interpret npts as filterData[].
            assert((void*)&(cfg().filterData[0]) == (void*)&npts);
            cfg().nFilterData = sizeof(int);
        }
        int npts;
    private:
        // No copying or assignment
        A2DBoxcarConfig(const A2DBoxcarConfig& x);
        A2DBoxcarConfig& operator=(const A2DBoxcarConfig& rhs);
    };

    /**
     * A2D configuration for time-based averaging of A2D samples.
     * filterData[] contains the desired output rate.
     */
    class A2DTimeAvgConfig: public A2DSampleConfig
    {
    public:
        A2DTimeAvgConfig(int n): A2DSampleConfig(),rate(n)
        {
            // make sure there is no padding or extra bytes
            // between the end of nidas_a2d_sample_config and npts.
            // The driver C code will interpret npts as filterData[].
            assert((void*)&(cfg().filterData[0]) == (void*)&rate);
            cfg().nFilterData = sizeof(int);
        }
        int rate;
    private:
        // No copying or assignment
        A2DTimeAvgConfig(const A2DTimeAvgConfig& x);
        A2DTimeAvgConfig& operator=(const A2DTimeAvgConfig& rhs);
    };

    std::vector<A2DSampleConfig*> _sampleCfgs;

    /**
     * Information needed to intepret the samples that are
     * received from the A2D device.
     */
    class A2DSampleInfo
    {
    public:
        A2DSampleInfo(int n)
            : nvars(n),nvalues(0),stag(0),channels(nvars) {}
        A2DSampleInfo(const A2DSampleInfo& x): nvars(x.nvars),nvalues(x.nvalues),
            stag(x.stag),channels(x.channels)
        {
        }
        A2DSampleInfo& operator= (const A2DSampleInfo& rhs)
        {
            if (&rhs != this) {
                nvars = rhs.nvars;
                nvalues = rhs.nvalues;
                stag = rhs.stag;
                channels = rhs.channels;
            }
            return *this;
        }

        ~A2DSampleInfo() { }
        int nvars;
        int nvalues;
        SampleTag* stag;
        std::vector<int> channels;
    };

    std::vector<A2DSampleInfo> _sampleInfos;

    /**
     * Counter of number of raw samples of wrong size.
     */
    size_t _badRawSamples;

private:
    /**
     * Requested A2D sample rate before decimation.
     */
    int _scanRate;

    int _prevChan;

    /** No copying */
    A2DSensor(const A2DSensor&);

    /** No assignment */
    A2DSensor& operator=(const A2DSensor&);
};

}}	// namespace nidas namespace dynld

#endif
