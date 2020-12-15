// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_TIMETADJUSTER_H
#define NIDAS_CORE_TIMETADJUSTER_H

#include "SampleTag.h"
#include "DSMSensor.h"

namespace nidas { namespace core {

class SampleTracer;

/**
 * Adjust time tags of fixed delta-T time series to correct
 * for irregular latency in the assignments of the time tags
 * at acquisition time.
 *
 * See nidas/doc/timetags.md for a discussion of the algorithm.
 */

class TimetagAdjuster {
public:

    /**
     * Constructor
     * @param rate Sample rate in Hz, sec^-1.
     */
    TimetagAdjuster(dsm_sample_id_t id, double rate);

    /**
     * Adjust a time tag.
     * @param tt NIDAS time tag to be adjusted.
     * @return Adjusted time tag.
     */
    dsm_time_t adjust(dsm_time_t tt);

    /**
     * Log various statistics of the TimetagAdjuster, used by sensors classes
     * on shutdown.
     */
    void log(int level, const DSMSensor* sensor, bool octalLable=false);

    /**
     * Log message for a traced sample. Used for high-rate messages containing
     * each time tag and tdiff.
     */
    void slog(SampleTracer& stracer, const std::string& msg, dsm_time_t tt,
        long long toff, int tdiff, int tdiffUncorr);

    /**
     * Log message for a traced sample. Used for lower-rate messages every
     * _npts.
     */
    void slog(SampleTracer& stracer, const std::string& msg, dsm_time_t tt);

    /**
     * Return the configured sample rate, as passed to constructor.
     */
    float getRate() const { return USECS_PER_SEC / (float) _dtUsec; }

    /**
     * Return the maximum dt in the adjusted time tags. This is the
     * maximum difference between successive, adjusted time tags.
     */
    float getMaxResultDt() const { return (float)_dtResultMax / USECS_PER_SEC; }

    /**
     * Return the minimum dt in the adjusted time tags.
     */
    float getMinResultDt() const { return (float)_dtResultMin / USECS_PER_SEC; }

    /**
     * Return the current average input delta-T so far.
     * 1.0 / getDtAvg() is the observed sample rate.
     */
    double getDtAvg() const { return _dtUsecCorrSum / _nCorrSum / USECS_PER_SEC; }

    /**
     * Return the minimum calculated delta-T so far.  The observed delta-T
     * of the input raw time tags is averaged to get a better estimate
     * of the actual dt , because it will not typically be exactly the
     * inverse of the configured rate.
     */
    float getDtMin() const { return (float)_dtUsecCorrMin / USECS_PER_SEC; }

    /**
     * Return the maximum calculated delta-T so far.
     */
    float getDtMax() const { return (float)_dtUsecCorrMax / USECS_PER_SEC; }

    /**
     * Return the maximum adjustment to the measured time tags.
     */
    float getAdjMax() const { return (float)_tadjMaxUsec / USECS_PER_SEC; }

    /**
     * Return the minimum adjustment to the measured time tags.
     */
    float getAdjMin() const { return (float)_tadjMinUsec / USECS_PER_SEC; }

    /**
     * Maximum data gap in the non-adjusted, raw, time tags, in seconds.
     */
    float getMaxGap() const { return (float) _maxGap / USECS_PER_SEC; }

    /**
     * Total number of time tags processed.
     */
    unsigned int getNumPoints() const { return _ntotalPts; }

    /**
     * How many backwards input time tags have been encountered, typically zero.
     */
    unsigned int getNumBackwards() const { return _nBack; }

    static const int BIG_GAP_SECONDS = 10;

private:
   
    /**
     * Result time tags will have a integral number of delta-Ts
     * from this base time. This base time is slowly adjusted
     * by computing the minimum difference between
     * the result time tags and the input time tags.
     */
    dsm_time_t _tt0;

    /**
     * Previous time tag.
     */
    dsm_time_t _tlast;

    /**
     *
     */
    dsm_time_t _ttnDt0;

    dsm_time_t _ttAdjLast;

    long long _maxGap;

    /**
     * Sum for average of dt.
     */
    double _dtUsecCorrSum;

    /**
     * ID of samples that are adjusted, used in log messages.
     */
    dsm_sample_id_t _id;

    /**
     * Expected delta-T in microseconds, 1/rate.
     */
    unsigned int _dtUsec;

    /**
     * Corrected delta-T in microseconds.
     */
    unsigned int _dtUsecCorr;

    /**
     * Current number of delta-Ts from tt0.
     */
    unsigned int _nDt;

    /**
     * Minimum number of points to compute minimum time difference.
     */
    unsigned int _npts;

    /**
     * Minimum diffence between actual time tags and expected, over _npts.
     */
    int _tdiffminUsec;

    /**
     * Minimum averaged dt so far.
     */
    int _dtUsecCorrMin;

    /**
     * Maximum averaged dt so far.
     */
    unsigned int _dtUsecCorrMax;

    unsigned int _nCorrSum;

    unsigned int _nBack;

    int _tadjMinUsec;
    unsigned int _tadjMaxUsec;

    unsigned int _ntotalPts;


    int _dtResultMin;
    int _dtResultMax;

    int _tdiffLast;

    unsigned int _nNegTdiff;

    unsigned int _nBigTdiff;

    int _nworsen;
    int _nimprove;

    /**
     * Number of samples in 5 minutes, used for running average.
     */
    unsigned int _nSamp5Min;
};

}}	// namespace nidas namespace core

#endif

