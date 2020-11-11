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

/**
 * Adjust time tags of fixed delta-T time series to correct
 * for irregular latency in the assignments of the time tags
 * at acquisition time.
 *
 * NIDAS basically assigns time tags to samples by reading the
 * system clock at the moment the sample is acquired. It is a
 * little more complicated than this for certain types of sensors,
 * such as SerialSensor, where the time tag is adjusted for an estimated
 * RS232 transmission time, but basically the time tag assigned to a
 * raw sample is the best estimate available of the moment the first
 * byte of the sample is acquired.
 *
 * This assignment suffers from system latency, such that in general
 * the time tags are later than they should be, with some jitter.
 * This adjuster reduces the jitter in the sampling latencies.
 *
 * The archived, original time tags, ttorig[i], are passed one
 * at a time to the adjust() method.
 *
 * Each time adjust() is called, it computes an estimated
 * time tag, tt[I], using a fixed time delta from a base time,
 * tt[0]:
 *      I++
 *      tt[I] = tt[0] + I * dt
 * This value of tt[I] is returned as the adjusted time tag
 * from the adjust() method.
 *
 * The trick is to figure out what tt[0] and dt are. To start off, and
 * after any gap or backwards time in the time series, tt[0] is
 * reset to the original time tag passed to adjust():
 *      I = 0
 *      tt[I] = ttorig[i]
 * and the unadjusted time tag, tt[I], is returned from
 * adjust().
 *
 * Then over the next adjustment period, the difference between
 * each measured and estimated time tag is computed:
 *      tdiff = ttorig[i] - tt[I]
 * tdiff is an estimate of any additional latency in the assignment
 * of the original time tag.  Over the adjustment period, the minimum
 * of these latencies is computed. This minimum latency is used to compute
 * the new value of tt[0] at the beginning of the next adjustment
 * period:
 *      I = 0
 *      tt[I] = tt[N] + tdiffmin
 * where tt[N] is the last estimated time generated in the previous
 * adjustment period.
 *
 * tdiff min is also used for a small correction to dt:
 *      dt = dt + tdiffmin / N
 * This correction to dt  corrects for a sensor clock that may
 * not be exactly correct, so that the measured dt is different
 * from what would be expected as dt=1/rate.
 *
 * Using this simple method one can account for a slowly drifting
 * sensor clock, where (hopefully small) step changes are made after
 * every adjustment period to correct the accumulated time tag error
 * when the actual time delta of the samples differs from the stated dt.
 *
 * This adjustment algorithm assumes that the system clock is much
 * more accurate than the variable sampling latency of the system,
 * which is generally true when the data system is synced to a
 * NTP stratum 1-3 server over a good network link, or to a directly
 * connected reference clock, such as a GPS with a PPS.
 *
 * This method works if there are times during the adjustment period
 * where the system is able to respond very quickly to the receipt
 * of a sample and assign its time tag. This minimum latency is
 * determined and used to compute the base time, tt[0], of the next
 * time series.
 *
 * The adjustment period is passed to the constructor, and is typically
 * something like one second for sample rates over 1, and perhaps 10 seconds
 * for a sample rate of 1 sps.
 *
 * The adjustment is reset as described above on a backwards time in
 * the original time series or on a data gap.
 *
 */

class TimetagAdjuster {
public:

    /**
     * Constructor
     * @param rate Sample rate in Hz, sec^-1.
     * @param adjustSeconds Number of seconds to compute minimum
     *      latency jitter.
     * @param sampleGap Fractional number of sample time deltas
     *      to consider a gap, and a reset of the adjustment.
     */
    TimetagAdjuster(double rate, float adjustSecs, float sampleGap);

    /**
     * Adjust a time tag.
     * @param tt NIDAS time tag to be adjusted.
     * @return Adjusted time tag.
     */
    dsm_time_t adjust(dsm_time_t tt, bool debug=false);

    /**
     * Log various statistics of the TimetagAdjuster.
     */
    void log(int level, const DSMSensor* sensor, dsm_sample_id_t id,
            bool octalLable=true);

    /**
     * Return the number of times an adjustment to a time tag has
     * been greater than the configured delta-T.
     */
    unsigned int getNumLargeAdjust() const { return _nLargeAdjust; }

    /**
     * Return the number of ttadjust results so far.
     * A reset of the ttadjust algrorithm happens when a measured
     * delta-T (difference between a time tag and the previous)
     * is greater than sampleGap seconds.
     */
    unsigned int getNumBackwards() const { return _nBack; }

    unsigned int getNumGaps() const { return _nGap; }

    /**
     * Return the configured sample rate, as passed to constructor.
     */
    float getRate() const { return USECS_PER_SEC / (float) _dtUsec; }

    /**
     * Return the minimum calculated delta-T so far.
     */
    float getDtMin() const { return (float)_dtUsecCorrMin / USECS_PER_SEC; }

    /**
     * Return the maximum calculated delta-T so far.
     */
    float getDtMax() const { return (float)_dtUsecCorrMax / USECS_PER_SEC; }

    /**
     * Return the average calculated delta-T so far.  1.0 / getDtAvg() is the
     * calculated sample rate.
     */
    double getDtAvg() const { return _dtUsecCorrSum / _nCorrSum / USECS_PER_SEC; }

    /**
     * Return the minimum adjustment to the measured time tags.
     */
    float getAdjMin() const { return (float)_tadjMinUsec / USECS_PER_SEC; }

    /**
     * Return the maximum adjustment to the measured time tags.
     */
    float getAdjMax() const { return (float)_tadjMaxUsec / USECS_PER_SEC; }

    /**
     * Total number of time tags processed.
     */
    unsigned int getNumPoints() const { return _ntotalPts; }

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
     * Expected delta-T in microseconds.
     */
    unsigned int _dtUsec;

    /**
     * Corrected delta-T in microseconds.
     */
    unsigned int _dtUsecCorr;

    /**
     * Adjustment period, how long to calculate the minimum
     * latency jitter before applying the correction.
     */
    unsigned int _adjustUsec;

    /**
     * Current number of delta-Ts from tt0.
     */
    int _nDt;

    /**
     * How many points to compute minimum time difference.
     */
    int _npts;

    /**
     * Minimum diffence between actual time tags and expected.
     */
    int _tdiffminUsec;

    /**
     * A gap in the original time series more than this value
     * causes a reset in the computation of estimated time tags.
     */
    int _dtGapUsec;

    unsigned int _nLargeAdjust;

    unsigned int _dtUsecCorrMin;
    unsigned int _dtUsecCorrMax;
    double _dtUsecCorrSum;
    unsigned int _nCorrSum;

    unsigned int _nBack;
    unsigned int _nGap;

    int _tadjMinUsec;
    int _tadjMaxUsec;

    unsigned int _ntotalPts;

};

}}	// namespace nidas namespace core

#endif

