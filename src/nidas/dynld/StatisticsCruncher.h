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

#ifndef NIDAS_DYNLD_STATISTICSCRUNCHER_H
#define NIDAS_DYNLD_STATISTICSCRUNCHER_H

#include <nidas/core/SampleSource.h>
#include <nidas/core/SampleClient.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/core/NearestResampler.h>
#include <nidas/util/UTime.h>

#include <vector>
#include <algorithm>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 */
class StatisticsCruncher : public Resampler
{
public:

    /**
     * Types of statistics I can generate.
     */
    typedef enum statsEnumType {
        STATS_UNKNOWN,STATS_MINIMUM,STATS_MAXIMUM,STATS_MEAN,STATS_VAR,
	STATS_COV, STATS_FLUX, STATS_RFLUX,STATS_SFLUX,
        STATS_TRIVAR,STATS_PRUNEDTRIVAR
    } statisticsType;

    /**
     * Constructor.
     */
    StatisticsCruncher(const SampleTag* stag,statisticsType type,
    	std::string countsName,bool higherMoments, const Site*);

    ~StatisticsCruncher();

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

    bool receive(const Sample *s) throw();

    /**
     * Flush the last sample from the stats.
     */
    void finish() throw();

    /**
     * Connect a SamplePipeline to the cruncher.
     */
    void connect(SampleSource* source) throw(nidas::util::InvalidParameterException);

    void disconnect(SampleSource* source) throw();

    /**
     * Connect a SamplePipeline to the cruncher.
     */
    void connect(SampleOutput* output);

    void disconnect(SampleOutput* output);

    static statisticsType getStatisticsType(const std::string& type)
    	throw(nidas::util::InvalidParameterException);

    void setStartTime(const nidas::util::UTime& val) 
    {
        _startTime = val;
    }

    nidas::util::UTime getStartTime() const
    {
        return _startTime;
    }

    void setEndTime(const nidas::util::UTime& val) 
    {
        _endTime = val;
    }

    nidas::util::UTime getEndTime() const
    {
        return _endTime;
    }

    long long getPeriodUsecs() const 
    {
        return _periodUsecs;
    }

    /**
     * Whether to generate output samples over time gaps.
     * In some circumstances one might be generating statistics
     * for separate time periods, and one does not want
     * to output samples of missing data for the gaps between
     * the periods.
     */
    bool getFillGaps() const 
    {
        return _fillGaps;
    }

    void setFillGaps(bool val)
    {
        _fillGaps = val;
    }

protected:

    void attach(SampleSource* source) throw(nidas::util::InvalidParameterException);

    /**
     * Split input variable names at periods.
     */
    void splitNames();

    /**
     * Create a derived variable name from input variables i,j,k,l.
     * Specify j,k,l for 2nd,3rd,4th order products.
     */
    std::string makeName(int i, int j=-1, int k=-1, int l=-1);

    /**
     * Create a derived units field from input variables i,j,k,l.
     * Specify j,k,l for 2nd,3rd,4th order products.
     */
    std::string makeUnits(int i, int j=-1, int k=-1, int l=-1);

    std::string makeUnits(const std::vector<std::string>&);

    void createCombinations();

    void setupMoments(unsigned int nvars, unsigned int moment);

    void setupMinMax(const std::string&);

    void setupCovariances();

    void setupTrivariances();

    void setupPrunedTrivariances();

    void setupFluxes();

    void setupReducedFluxes();

    void setupReducedScalarFluxes();

    void initStats();

    void zeroStats();

    void computeStats();

private:

    /**
     * Add a SampleTag to this SampleSource. This SampleSource
     * does not own the SampleTag.
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

    std::vector<Variable*> _reqVariables;
    
    /**
     * Number of input variables.
     */
    unsigned int _nvars;

    /**
     * Name of counts variable.
     */
    std::string _countsName;

    /**
     * Does the user want number-of-points output?
     */
    bool _numpoints;

    dsm_time_t _periodUsecs;

    /**
     * Does this cruncher compute cross-terms?
     */
    bool _crossTerms;

    NearestResampler* _resampler;

    /**
     * Types of statistics I can generate.
     */
    statisticsType _statsType;

    std::vector<std::vector<std::string> > _splitVarNames;

    int _nwordsSuffix;

    SampleTag _outSample;

    unsigned int _nOutVar;

    unsigned int _outlen;

    dsm_time_t _tout;

    struct sampleInfo {
        sampleInfo(): weightsIndex(0),varIndices() {}
        unsigned int weightsIndex;
	std::vector<unsigned int*> varIndices;
    };

    std::map<dsm_sample_id_t,sampleInfo > _sampleMap;

    float* _xMin;

    float* _xMax;

    // statistics sums.
    double* _xSum;

    double** _xySum;
    double* _xyzSum;
    double* _x4Sum;

    unsigned int *_nSamples;

    unsigned int **_triComb;

    /**
     * Number of covariances to compute.
     */
    unsigned int _ncov;

    /**
     * Number of trivariances to compute.
     */
    unsigned int _ntri;

    /**
     * Number of 1st,2nd,3rd,4th moments to compute.
     */
    unsigned int _n1mom,_n2mom,_n3mom,_n4mom;

    /**
     * Total number of products to compute.
     */
    unsigned int _ntot;

    bool _higherMoments;

    const Site* _site;

    nidas::util::UTime _startTime;

    nidas::util::UTime _endTime;

    bool _fillGaps;

    /** No copy.  */
    StatisticsCruncher(const StatisticsCruncher&);

    /** No assignment.  */
    StatisticsCruncher& operator=(const StatisticsCruncher&);

};

}}	// namespace nidas namespace core

#endif
