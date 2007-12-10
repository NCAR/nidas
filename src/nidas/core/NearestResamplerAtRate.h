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
#include <nidas/core/SampleInput.h>
#include <nidas/core/DSMTime.h>

#include <vector>

namespace nidas { namespace core {

/**
 * A simple, nearest-point resampler, for generating merged
 * samples from variables from one or more sample sources.
 * The output sample time tags will be evenly spaced at the
 * given rate, and values of other variables are merged
 * into the output sample by associating those values with
 * the nearest time tag to the output time tags.
 *
 * The only requirement is that the samples which are fed
 * to the receive() method should be sorted in time. It they
 * aren't sorted some data will be lost.
 * NearestResamplerAtRate does not need to know sampling
 * rates, and the sampling rates of the input variables, including
 * the master variable, may vary.
 */
class NearestResamplerAtRate : public Resampler {
public:

    /**
     * Constructor.
     */
    NearestResamplerAtRate(const std::vector<const Variable*>& vars);

    NearestResamplerAtRate(const std::vector<Variable*>& vars);

    ~NearestResamplerAtRate();

    void setRate(float val)
    {
        rate = val;
        deltatUsec = (long long)(USECS_PER_SEC / rate);
        deltatUsec10 = deltatUsec / 10;
    }

    float getRate() const
    {
        return rate;
    }

    void setFillGaps(bool val)
    {
        fillGaps = val;
    }

    bool getFillGaps() const
    {
        return fillGaps;
    }

    bool receive(const Sample *s) throw();

    /**
     * Flush the last sample from the resampler.
     */
    void finish() throw();

    /**
     * Get the SampleTag of my merged output sample.
     */
    const std::list<const SampleTag*>& getSampleTags() const
    {
        return sampleTags;
    }

    /**
     * Connect the resampler to an input.
     */
    void connect(SampleInput* input) throw(nidas::util::IOException);

    void disconnect(SampleInput* input) throw(nidas::util::IOException);

private:

    void sendSample(dsm_time_t) throw();

    /**
     * Common tasks of constructors.
     */
    void ctorCommon(const std::vector<const Variable*>& vars);

    std::list<const SampleTag*> sampleTags;

    SampleTag outSampleTag;

    int nvars;

    int outlen;

    std::map<dsm_sample_id_t,std::vector<int*> > sampleMap;

    float rate;

    int deltatUsec;

    int deltatUsec10;

    dsm_time_t outputTT;

    dsm_time_t nextOutputTT;

    dsm_time_t* prevTT;

    dsm_time_t* nearTT;

    float* prevData;

    float* nearData;

    int* samplesSinceOutput;

    SampleT<float>* osamp;

    bool fillGaps;

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
