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

    void setRate(float val);

    float getRate() const
    {
        return _rate;
    }

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

    float _rate;

    int _deltatUsec;

    int _deltatUsec10;

    bool _exactDeltatUsec;

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
