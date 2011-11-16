// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_CORE_ADAPTIVE_DESPIKER_H
#define NIDAS_CORE_ADAPTIVE_DESPIKER_H

#include <stdexcept>

namespace nidas { namespace core {

/**
 * Adaptive forecaster for despiking of time-series data.
 * Algorithim taken from "A Statistical Data Screening Procedure"
 * by Jorgen Hojstrup, Dept of Meteorology and Wind Energy,
 * RISO National Laboratory, Denmark.
 * [Published as: Meas. Sci. Technol (UK). v4 (1993) p. 153-157]
 */
class AdaptiveDespiker
{

public:
    /**
     * Constructor.
     */
    AdaptiveDespiker();

    /**
     * @param val Probability of an outlier.
     * This is converted to a discrimination level
     * based on a Gaussian distribution.
     * @see 

     */
    void setOutlierProbability(float val);

    float getOutlierProbability() const { return _prob; }

    /**
     * @param val Discrimination level fudge factor.
     * The computation of discrimination level from
     * the Gaussian probability is typically too small
     * and one can multiply the resultant level by
     * this factor. We've typically used 2.5 in practice.
     */
    void setDiscLevelMultiplier(float val);

    float getDiscLevelMultiplier() const { return _levelMultiplier; }

    /**
     * If a value is more than discrLevel * sigma away 
     * from the mean, then it is considered a spike, i.e.
     * it's probability exceeded getOutlierProbability().
     */
    float getDiscLevel() const { return _level; }

    /**
     * Pass a value u, and return a forecasted value,
     * along with a boolean indicating whether AdaptiveDespiker
     * considers it a spike.  The input value is added
     * to the AdaptiveDespiker statistics for forecasting.
     */
    float despike(float u, bool* spike);

    /**
     * Forecast a value based on the previous point
     * and correlation and mean.
     */
    float forecast() const
    {
	return _u1 * _corr + (1. - _corr) * _mean2;
    }

    /**
     * Reset the statistics.
     */
    void reset();

    /**
     * Compute a discrimination level from a Gaussan probability.
     * If a value is more than discrLevel * sigma away 
     * from the mean, then it is considered a spike, i.e.
     * it's probability exceeded the given value.
     */
    static float discrLevel(float prob);

    /**
     * Adjust the discrimination level based on the current
     * correlation.
     */
    static float adjustLevel(float corr);

private:

    static bool staticInit();

    static const size_t STATISTICS_SIZE = 100;

    static const int ADJUST_TABLE_SIZE = 100;

    static const int LEN_ERFC_ARRAY = 140;

    static float _adj[ADJUST_TABLE_SIZE][2];

    static bool staticInitDone;

    static void spline(float* x,float* y,int n,double yp1,double ypn,float* y2);

    static double splint (float* xa,float* ya,float* y2a,int n, double x)
    	throw(std::range_error);

    void initStatistics(float u);

    void incrementStatistics(float u);

    void updateStatistics(float u);


    /** Prob for detection of outlier in good Gaussian data */
    float _prob;

    /** Increase detection level with this factor */
    float _levelMultiplier;

    /**
     * If more than _maxMissingFreq of the recent 10 data points are
     * missing data points, don't substitute forecasted data or
     * update forecast statistics.
     */
    float _maxMissingFreq;

    /** Last point. */
    float _u1;	

    /** Previous mean. */
    double _mean1;

    /** Current mean. */
    double _mean2;

    /** Previous variance. */
    double _var1;

    /** Current variance. */
    double _var2;


    /** Correlation */
    double _corr;

    /** Discrimination level */
    double _initLevel;

    double _level;

    /** Running ave of freq of missing data */
    float _missfreq;

    /** Memory size */
    int _msize;

    /** Number of points processed */
    size_t _npts;
};

}}	// namespace nidas namespace core

#endif
