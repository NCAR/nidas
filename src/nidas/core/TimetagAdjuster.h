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
     * Check a delta-T against expected limits.
     * Return whether to use the delta-T or not.
     */
    bool screenDt(dsm_time_t tt, double dtUsec);

    /**
     * Log various statistics of the TimetagAdjuster, used by sensors classes
     * on shutdown.
     */
    void log(int level, const DSMSensor* sensor, bool octalLable=false);

    /**
     * Log message for a traced sample. Used for high-rate messages containing
     * each time tag and latency.
     */
    void slog(SampleTracer& stracer, const std::string& msg, dsm_time_t tt,
        int dtAdj, int latency);

    /**
     * Log message for a traced sample. Used for lower-rate messages every
     * N_AVG.
     */
    void slog(SampleTracer& stracer, const std::string& msg, dsm_time_t tt, double dt);

    /**
     * Return the configured sample rate, as passed to constructor.
     */
    float getRate() const { return USECS_PER_SEC / _dtUsecConfig; }

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
     * Return the overall average input delta-T.
     */
    double getDtAvg() const { return _dtUsecAvgSum / _nDtAvgSum / USECS_PER_SEC; }

    /**
     * Return the observed average sample rate.
     */
    float getAvgRate() const { return  1.0 / getDtAvg(); }

    /**
     * Return the minimum averaged delta-T so far.
     */
    float getDtMin() const { return _dtUsecAvgMin / USECS_PER_SEC; }

    /**
     * Return the maximum averaged delta-T so far.
     */
    float getDtMax() const { return _dtUsecAvgMax / USECS_PER_SEC; }

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

    /**
     * Return the maximum adjustment applied to the time tags.
     */
    float getLatencyMax() const { return (float)_latencyMaxUsec / USECS_PER_SEC; }

    /**
     * Return the minimum adjustment to the measured time tags.
     */
    float getLatencyMin() const { return (float)_latencyMinUsec / USECS_PER_SEC; }

private:

    /**
     * Parameters for TimetagAdjuster.
     * The values are currently set in the Parameters constructor.
     * If they need to be tuned for a given project or sensor, the
     * could be added to the XML configuration.
     */
    class Parameters {
    public:

        Parameters(double rate);

        /**
         * On encounter of a gap of this size, reset the TimetagAdjuster.
         */
        unsigned int BIG_GAP_USEC;

        /**
         * Adjust _tt0 every N_ADJ samples.
         */
        unsigned int N_ADJ;

        /**
         * Average period for _dtUsecAvg, in number of samples.
         */
        unsigned int N_AVG;

        /**
         * Screen observed delta-Ts to be within this fraction of
         * the configured delta-T = 1/rate.
         */
        float DT_LIMIT_FRACTION;

        /**
         * If more than this number of consecutive delta-T's fail
         * the limit test, then reset the limits.
         */
        int MAX_CONSEQ_BAD_DT;

        /**
         * Warn if the minimum latency over N_ADJ samples
         * is larger than LATENCY_LIMIT times the delta-T.
         */
        float LATENCY_WARN_DT;

        /**
         * If the estimated latency worsens consecutive for
         * this number of samples, reset the adjustment.
         */
        int LATENCY_WORSEN_MAX;

    } _params;

    /**
     * ID of samples to be adjusted, used in log messages.
     */
    dsm_sample_id_t _id;

    /**
     * Configured delta-T in usecs:  USECS_PER_SEC / rate,
     * where rate is the configured attribute of <sample>.
     */
    double _dtUsecConfig;

    /**
     * Number of samples processed.
     */
    unsigned int _ntotalPts;

    /**
     * Previous time tag.
     */
    dsm_time_t _ttlast;

    /**
     * Number of backwards sample times.
     */
    unsigned int _nBack;

    /**
     * Number of gaps encountered.
     */
    unsigned int _ngap;

    /**
     * Maximum data gap found.
     */
    long long _maxGap;

    /**
     * Compute result time tags as an integral number of delta-Ts
     * from this zero time. This zero time is reset every N_ADJ
     * samples using the minimum difference found between
     * the result time tags and the input time tags.
     */
    dsm_time_t _tt0;

    /**
     * Current number of delta-Ts from tt0.
     */
    unsigned int _nDt;

    /**
     * First time tag in N_ADJ samples.
     */
    dsm_time_t _ttnDt0;

    /**
     * Corrected delta-T in microseconds.
     */
    double _dtUsecAvg;

    /**
     * Sum for average of dt.
     */
    double _dtUsecAvgSum;

    unsigned int _nDtAvgSum;

    /**
     * Low value for screening observed delta-T.
     */
    double _dtUsecLowLimit;

    /**
     * High value for screening observed delta-T.
     */
    double _dtUsecHighLimit;

    unsigned int _nDtLow;

    unsigned int _nDtHigh;

    /**
     * Number of consecutive low delta-Ts.
     */
    int _maxConsecLow;

    /**
     * Number of consecutive high delta-Ts.
     */
    int _maxConsecHigh;

    /**
     * Minimum averaged dt so far.
     */
    double _dtUsecAvgMin;

    /**
     * Maximum averaged dt so far.
     */
    double _dtUsecAvgMax;

    int _dtResultMin;

    int _dtResultMax;

    /**
     * Minimum diffence between actual time tags and expected, over N_ADJ
     * number of samples.
     */
    int _latencyAdjUsec;

    /**
     * Number of applied latencies larger than LATENCY_FILTER * deltaT.
     */
    unsigned int _nLargeLatency;

    /**
     * Latency of previous sample.
     */
    int _latencyLast;

    unsigned int _nNegLatency;

    dsm_time_t _ttAdjLast;

    int _latencyMinUsec;

    int _latencyMaxUsec;

};

}}	// namespace nidas namespace core

#endif

