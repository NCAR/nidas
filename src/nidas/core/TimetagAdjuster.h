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

#include "Sample.h"

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
 * The trick is to figure out what tt[0] is. To start off, and
 * after any gap or backwards time in the time series, tt[0] is
 * reset to the original time tag passed to adjust():
 *      I = 0
 *      tt[I] = ttorig[i]
 * and the unadjusted time tag, tt[I], is returned from
 * adjust().
 *
 * Then over the next adjustment period, the difference between
 * the original and estimated time tag is computed:
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
 * Using this simple method one can account for a slowly drifting
 * sensor clock, where (hopefully small) step changes are made after
 * every adjustment period to correct the accumulated time tag error
 * when the actual time delta of the samples differs from the stated dt.
 *
 * This method works if there are times during the adjustment period
 * where the system is able to respond very quickly to the receipt
 * of a sample and assign its time tag. This minimum latency is
 * determined and used to compute the base time, tt[0], of the next
 * time series.
 *
 * The adjustment period is passed to the constructor, and is typically
 * something like 10 seconds.
 *
 * The adjustment is reset as described above on a backwards time in
 * the original time series or on a data gap.
 *
 */

class TimetagAdjuster {
public:

    TimetagAdjuster(double rate, float adjustSecs);

    dsm_time_t adjust(dsm_time_t);

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
     * Actual delta-T in microseconds.
     */
    unsigned int _dtUsecActual;

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

};

}}	// namespace nidas namespace core

#endif

