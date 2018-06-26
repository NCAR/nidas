// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef  NIDAS_UTIL_BASICRUNNINGSTATS_H
#define  NIDAS_UTIL_BASICRUNNINGSTATS_H

#include <cmath>
#include <limits>

namespace nidas { namespace util {

class BasicRunningStats 
{
public: 
    struct StatResults {
        StatResults() : _min(0), _max(0), _mean(0), _stddev(0) {}
        StatResults(double min, double max, double mean, double stddev)
            : _min(min), _max(max), _mean(mean), _stddev(stddev) {}
        double _min;
        double _max;
        double _mean;
        double _stddev;
    };

    BasicRunningStats () : _numSamples(0), _min(std::numeric_limits<double>::max()), _max(0), 
                           _mean(0), _m2(0), _stddev(0), _variance(0) {}
    ~BasicRunningStats() {}

    const StatResults updateStats(double newSample) {
        _min = fmin(newSample, _min);
        _max = fmax(newSample, _max);

        ++_numSamples;
        double _delta = newSample - _mean;
        _mean += _delta/_numSamples;
        _m2 += _delta * (newSample - _mean);
        _variance = _m2/_numSamples;
        _stddev = sqrt(_variance);

        return StatResults(_min, _max, _mean, _stddev);
    }

    const StatResults getStats()
    {
        return StatResults(_min, _max, _mean, _stddev);
    }

    void resetStats()
    {
        _numSamples = 0;
        _min = std::numeric_limits<double>::max();
        _max = 0;
        _mean = 0;
        _m2 = 0;
        _stddev = 0;
        _variance = 0;
    }

private:
    unsigned long long _numSamples;
    double _min;
    double _max;
    double _mean;
    double _m2;
    double _stddev;
    double _variance;

    // no copying...
    BasicRunningStats(const BasicRunningStats& rRight);
    BasicRunningStats(BasicRunningStats& rRight);
    const BasicRunningStats& operator=(const BasicRunningStats& rRight);
    BasicRunningStats& operator=(BasicRunningStats& rRight);
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_BASICRUNNINGSTATS_H

