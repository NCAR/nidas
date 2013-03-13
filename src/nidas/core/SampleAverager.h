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

#ifndef NIDAS_CORE_SAMPLEAVERAGER_H
#define NIDAS_CORE_SAMPLEAVERAGER_H

#include <nidas/core/Resampler.h>
#include <nidas/core/SampleTag.h>

namespace nidas { namespace core {

class Variable;

class SampleAverager : public Resampler {
public:

    SampleAverager();

    SampleAverager(const std::vector<const Variable*>& vars);

    SampleAverager(const std::vector<Variable*>& vars);

    virtual ~SampleAverager();

    /**
     * Set average period.
     * @param val average period, in seconds.
     */
    void setAveragePeriodSecs(float val) {
        _averagePeriodUsecs = (int)rint((double)val * USECS_PER_SEC);
	_outSample.setRate(val);
    }

    /**
     * Get average period.
     * @return average period, in seconds.
     */
    float getAveragePeriodSecs() const { return (double)_averagePeriodUsecs / USECS_PER_SEC; }

    void addVariable(const Variable *var);

    void addVariables(const std::vector<const Variable*>&);

    SampleSource* getRawSampleSource() { return 0; }

    SampleSource* getProcessedSampleSource() { return &_source; }

    /**
     * Get the output SampleTags.
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
    void addSampleClientForTag(SampleClient* client,const SampleTag*) throw()
    {
        // I only have one tag, so just call addSampleClient()
        _source.addSampleClient(client);
    }

    void removeSampleClientForTag(SampleClient* client,const SampleTag*) throw()
    {
        _source.removeSampleClient(client);
    }

    int getClientCount() const throw()
    {
        return _source.getClientCount();
    }

    /**
     * Implementation of Resampler::flush().
     */
    void flush() throw();

    const SampleStats& getSampleStats() const
    {
        return _source.getSampleStats();
    }

    /**
     * Connect the resampler to a SampleSource.
     */
    void connect(SampleSource* source) throw(nidas::util::InvalidParameterException);

    void disconnect(SampleSource* source) throw();

    bool receive(const Sample *s) throw();

protected:

    void init() throw();

private:
   
    /**
     * Add a SampleTag to this SampleSource.
     */
    void addSampleTag(const SampleTag* tag) throw ()
    {
        _source.addSampleTag(tag);
    }

    void removeSampleTag(const SampleTag* tag) throw ()
    {
        _source.removeSampleTag(tag);
    }

    SampleSourceSupport _source;

    SampleTag _outSample;

    /**
     * Length of average, in microseconds.
     */
    int _averagePeriodUsecs;

    /**
     * end time of current statistics window.
     */
    dsm_time_t _endTime;

    /**
     * Index of each requested output variable in the output sample.
     */
    std::map<Variable*,unsigned int> _outVarIndices;

    std::map<dsm_sample_id_t,std::vector<unsigned int> > _inmap;

    std::map<dsm_sample_id_t,std::vector<unsigned int> > _lenmap;

    std::map<dsm_sample_id_t,std::vector<unsigned int> > _outmap;

    unsigned int _ndataValues;

    double *_sums;

    int *_cnts;

    /**
     * No copy.
     */
    SampleAverager(const SampleAverager&);

    /**
     * No assignment.
     */
    SampleAverager& operator=(const SampleAverager&);

};

}}	// namespace nidas namespace core

#endif

