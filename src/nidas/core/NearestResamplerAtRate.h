// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_NEARESTRESAMPLERATRATE_H
#define NIDAS_CORE_NEARESTRESAMPLERATRATE_H

#include <nidas/core/Resampler.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/DSMTime.h>

#include <vector>

namespace nidas { namespace core {

/**
 * A simple, nearest-point resampler, for generating merged
 * samples from variables from one or more sample sources.
 * The output sample time tags will be evenly spaced at the
 * given rate, and values of variables are merged
 * into the output sample by associating those values with
 * the nearest time tag to the output time tags.
 *
 * The only requirement is that the samples which are fed
 * to the receive() method should be sorted in time. It they
 * aren't sorted some data will be lost.
 * NearestResamplerAtRate does not need to know sampling
 * rates, and the sampling rates of the input variables, including
 * the master variable, may vary.
 *
 * This resampler operates in two modes, based on the value
 * of the setMiddleTimeTags() attribute.
 *
 * If getMiddleTimeTags() is true, generate output timetags that are at
 * the middle of the requested output periods.
 * For example, for a rate=20, deltaT=0.05 sec, the output timetags will be
 * 00:00:00.025, 00:00:00.075, etc.  The output sample at 00:00:00.025
 * contains the nearest input values from the time period centered at 00:00:00.025.
 *
 * If getMiddleTimeTags() is false, the output time tags for the above example
 * would be 00:00:00.00, 00:00:00.05, etc.  The output sample at 00:00:00.00
 * contains the nearest input values from the time period centered at 00:00:00.00.
 *
 * Because of assumed input timetag jitter (inaccuracy), the nearest point
 * matching algorithm is a bit forgiving.  When matching for the nearest points
 * to time t, input samples will be matched whose time tags are between
 * t - 0.9*deltatT <= inputTimeTag <= t + 0.9*deltaT.
 * If more than one input sample lies in the window, then the nearest one is used.
 * This input window is of size 1.8 * deltaT, rather than 1.0 * deltaT,
 * which is what one might expect.  Therefore an input point could be matched
 * with two output points.  In this example, it is possible that an output sample
 * at 00:00:00.025 could contain an input value from the previous day.
 */
class NearestResamplerAtRate : public Resampler {
public:

    /**
     * Constructor.
     */
    NearestResamplerAtRate(const std::vector<const Variable*>& vars);

    NearestResamplerAtRate(const std::vector<Variable*>& vars);

    ~NearestResamplerAtRate();

    /**
     * Set the requested output rate, in Hz. For rates < 1 it is best to choose
     * a value such that 10^6/rate is an integer.  If you really want
     * rate=1/3 Hz, specify rate to 7 significant figures, 0.3333333, and you
     * will avoid round off errors in the time tag. 
     * Output rates > 1 should be integers, or of a value with enough significant
     * figures such that 10^6/rate is an integer. Support for other
     * rates could be added if (really) necessary.
     */
    void setRate(double val);

    double getRate() const
    {
        return _rate;
    }

    /**
     * If true, generate output timetags that are the middle of the
     *      requested output periods. For example, for a rate=20,
     *      deltaT=0.05 sec, the output timetags will be
     *      00:00:00.025, 00:00:00.075, etc.
     *      The sample at 00:00:00.025 contains the nearest input values
     *      from the time period centered at 00:00:00.025
     * If false, the output time tags for the above example would be
     *      00:00:00.00, 00:00:00.05, etc. The sample at 00:00:00.00
     *      contains the nearest input values from the time period centerd
     *      at 00:00:00.00, i.e. points from the previous day could be used.
     */
    void setMiddleTimeTags(bool val)
    {
        _middleTimeTags = val;
    }

    bool getMiddleTimeTags() const
    {
        return _middleTimeTags;
    }

    /**
     * Should output records of all missing data (nans), be generated, or just discarded.
     */
    void setFillGaps(bool val)
    {
        _fillGaps = val;
    }

    bool getFillGaps() const
    {
        return _fillGaps;
    }

    SampleSource* getRawSampleSource() { return 0; }

    SampleSource* getProcessedSampleSource() { return &_source; }

    /**
     * Get the SampleTag of my merged output sample.
     */
    std::list<const SampleTag*> getSampleTags() const
    {
        return _source.getSampleTags();
    }

    /**
     * Implementation of SampleSource::getSampleTagIterator().
     */
    SampleTagIterator getSampleTagIterator() const
    {
        return _source.getSampleTagIterator();
    }

    /**
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClient(SampleClient* client) throw()
    {
        _source.addSampleClient(client);
    }

    void removeSampleClient(SampleClient* client) throw()
    {
        _source.removeSampleClient(client);
    }

    /**
     * Add a Client for a given SampleTag.
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClientForTag(SampleClient* client,const SampleTag* tag) throw()
    {
        // I only have one tag, so just call addSampleClient()
        _source.addSampleClient(client);
    }

    void removeSampleClientForTag(SampleClient* client,const SampleTag* tag) throw()
    {
        _source.removeSampleClient(client);
    }

    int getClientCount() const throw()
    {
        return _source.getClientCount();
    }

    /**
     * Calls finish() all all SampleClients.
     * Implementation of SampleSource::flush().
     */
    void flush() throw()
    {
        _source.flush();
    }

    const SampleStats& getSampleStats() const
    {
        return _source.getSampleStats();
    }

    /**
     * Connect the resampler to a SampleSource.
     */
    void connect(SampleSource* input) throw(nidas::util::InvalidParameterException);

    void disconnect(SampleSource* input) throw();

    bool receive(const Sample *s) throw();

    /**
     * Flush the last sample from the resampler.
     */
    void finish() throw();

private:

    /**
     * Common tasks of constructors.
     */
    void ctorCommon(const std::vector<const Variable*>& vars);

    /**
     * Add a SampleTag to this SampleSource.
     */
    void addSampleTag(const SampleTag* tag) throw()
    {
        _source.addSampleTag(tag);
    }

    void removeSampleTag(const SampleTag* tag) throw ()
    {
        _source.removeSampleTag(tag);
    }

    void sendSample(dsm_time_t) throw();

    SampleSourceSupport _source;

    SampleTag _outSample;

    /**
     * Index of each requested output variable in the output sample.
     */
    std::map<Variable*,unsigned int> _outVarIndices;

    /**
     * For each input sample, first index of variable data values to be
     * read.
     */
    std::map<dsm_sample_id_t,std::vector<unsigned int> > _inmap;

    /**
     * For each input sample, length of variables to read.
     */
    std::map<dsm_sample_id_t,std::vector<unsigned int> > _lenmap;

    /**
     * For each input sample, index into output sample of each variable.
     */
    std::map<dsm_sample_id_t,std::vector<unsigned int> > _outmap;

    int _ndataValues;

    int _outlen;

    double _rate;

    /**
     * The output deltaT, 1/rate in microseconds.
     */
    int _deltatUsec;

    /**
     * DeltaT over 10. A fudge factur used for doing nearest point alignments.
     */
    int _deltatUsecD10;

    /**
     * DeltaT over 2.
     */
    int _deltatUsecD2;

    /**
     * True if simple integer math is used to increment
     * output sample time tags. This will be the case if
     * rate <= 1.0 or 1/rate is within 0.02 microseconds
     * if an integer.
     */
    bool _exactDeltatUsec;

    /**
     * If true, generate output timetags that are the middle of the
     *      requested output periods. For example, for a rate=20,
     *      deltaT=0.05 sec, the output timetags will be
     *      00:00:00.025, 00:00:00.075, etc.
     *      The sample at 00:00:00.025 contains the nearest
     *      input values from the period 00:00:00.0 to 00:00:00.05
     * If false, the output time tags for the above example would be
     *      00:00:00.00, 00:00:00.05, etc. The sample at 00:00:00.00
     *      contains the nearest input values from 23:59:59.975 to
     *      00:00:00.025.
     */
    bool _middleTimeTags;

    dsm_time_t _outputTT;

    dsm_time_t _nextOutputTT;

    dsm_time_t* _prevTT;

    dsm_time_t* _nearTT;

    float* _prevData;

    float* _nearData;

    int* _samplesSinceOutput;

    SampleT<float>* _osamp;

    bool _fillGaps;

    /**
     * No assignment.
     */
    NearestResamplerAtRate& operator=(const NearestResamplerAtRate&);

    /**
     * No copy.
     */
    NearestResamplerAtRate(const NearestResamplerAtRate& x);
};

}}	// namespace nidas namespace core

#endif
