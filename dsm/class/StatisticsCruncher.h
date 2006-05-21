/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-03-03 12:46:08 -0700 (Fri, 03 Mar 2006) $

    $LastChangedRevision: 3299 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:5080/svn/nids/branches/ISFF_TREX/dsm/class/SampleSorter.h $
 ********************************************************************

*/

#ifndef DSM_STATISTICSCRUNCHER_H
#define DSM_STATISTICSCRUNCHER_H

#include <SampleSource.h>
#include <SampleClient.h>
#include <SampleInput.h>
#include <NearestResampler.h>
#include <DSMTime.h>

#include <vector>

namespace dsm {

/**
 */
class StatisticsCruncher : public SampleClient, public SampleSource {
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
    StatisticsCruncher(const SampleTag* stag,statisticsType type, bool npts);

    /**
     * Copy constructor.  Making a copy is only valid
     * before the connections have been established.
     */
    StatisticsCruncher(const StatisticsCruncher& x);

    ~StatisticsCruncher();

    bool receive(const Sample *s) throw();

    /**
     * Flush the last sample from the stats.
     */
    void finish() throw();

    /**
     * Get the SampleTag of my merged output sample.
     */
    const std::set<const SampleTag*>& getSampleTags() const
    {
        return sampleTags;
    }

    /**
     * Connect an input to the cruncher.
     */
    void connect(SampleInput* input) throw(atdUtil::IOException);

    void disconnect(SampleInput* input) throw(atdUtil::IOException);

    static statisticsType getStatisticsType(const std::string& type)
    	throw(atdUtil::InvalidParameterException);
protected:

    void attach(SampleSource* src);

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

    void setupMoments(int nvars, int moment);

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

    std::vector<Variable*> inVariables;
    
    /**
     * Number of input variables.
     */
    int nvars;

    /**
     * Does the user want number-of-points output?
     */
    bool numpoints;

    dsm_time_t periodUsecs;

    /**
     * Does this cruncher compute cross-terms?
     */
    bool crossTerms;

    NearestResampler* resampler;

    /**
     * Types of statistics I can generate.
     */
    statisticsType statsType;

    std::vector<std::vector<std::string> > splitVarNames;

    int nwordsSuffix;

    std::set<const SampleTag*> sampleTags;

    SampleTag outSample;

    int outStation;

    unsigned int nOutVar;

    int outlen;

    dsm_time_t tout;

    struct sampleInfo {
        int weightsIndex;
	std::vector<int*> varIndices;
    };

    std::map<dsm_sample_id_t,sampleInfo > sampleMap;

    float* xMin;

    float* xMax;

    // statistics sums.
    double* xSum;

    double** xySum;
    double* xyzSum;
    double* x4Sum;

    int *nSamples;

    int **triComb;

    /**
     * Number of covariances to compute.
     */
    int ncov;

    /**
     * Number of trivariances to compute.
     */
    int ntri;

    /**
     * Number of 1st,2nd,3rd,4th moments to compute.
     */
    int n1mom,n2mom,n3mom,n4mom;

    /**
     * Total number of products to compute.
     */
    int ntot;
    /**
     * No assignment.
     */
    StatisticsCruncher& operator=(const StatisticsCruncher&);

};

}

#endif
