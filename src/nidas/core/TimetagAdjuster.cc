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

// #define DEBUG

#include "TimetagAdjuster.h"
#include <nidas/util/Logger.h>

#ifdef DEBUG
#include <nidas/util/UTime.h>
#endif

#include <cmath>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace std;

TimetagAdjuster::TimetagAdjuster(double rate, float sampleGap,
    float adjustPeriod):
    _tt0(LONG_LONG_MIN), _tlast(LONG_LONG_MIN), _ttNpt0(LONG_LONG_MIN),
    _dtUsec((int) ::lrint(USECS_PER_SEC / rate)),
    _dtUsecCorr(_dtUsec),
    // limit _dtGapUsec to less than an 1/2 hour
    _dtGapUsec(::lrint(std::min(sampleGap, 1800.f) * USECS_PER_SEC)),
    // limit _adjustUsec to less than an hour
    _adjustUsec(std::max((int)::lrint(std::min(adjustPeriod,3600.f) * USECS_PER_SEC), _dtGapUsec)),
    _nDt(0),
    _npts(_adjustUsec / _dtUsec),
    _tdiffminUsec(INT_MAX),
    _nLargeAdjust(0),
    _dtUsecCorrMin(INT_MAX), _dtUsecCorrMax(0),
    _dtUsecCorrSum(0.0), _nCorrSum(0),
    _nBack(0), _nGap(0),
    _tadjMinUsec(INT_MAX), _tadjMaxUsec(INT_MIN),
    _ntotalPts(0),
    _ttAdjLast(LONG_LONG_MIN),
    _maxGap(0)
{
}

dsm_time_t TimetagAdjuster::adjust(dsm_time_t tt, bool debug)
{
    if (_dtGapUsec <= 0) return tt;

    _ntotalPts++;
#ifdef DEBUG
    if (debug)
        cerr << "tt=" <<
            nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%5f") <<
            ", _tlast=" <<
            nidas::util::UTime(_tlast).format(true, "%Y %m %d %H:%M:%S.%5f") <<
            ", _tadjMaxUsec=" << _tadjMaxUsec << endl;
#endif
    if (tt < _tlast) {
        _nBack++;
        return tt;
    }
    if (tt > _tlast + _dtGapUsec) {
        _tt0 = tt;
        _nDt = 0;
        _tdiffminUsec = INT_MAX;
        if (_ntotalPts > 1) {
            _maxGap = std::max(_maxGap, tt - _tlast);
            _nGap++;
        }
        _ttNpt0 = tt;
        _tlast = tt;
        return tt;
    }
    int tdiff = tt - _tlast;
    _tlast = tt;
    _nDt++;

    /* Compute adjusted time, an integral number of dt's from _tt0.
     * _adjustUsec is a maximum of 3600 sec and so _nDt * _dtUsecCorr
     * will be less than 3600e6.  So toff as a 32 bit unsigned int
     * should be large enough: 2^32 = 4294e6, over an hour.
     */
    unsigned int toff = _nDt * _dtUsecCorr;

    /* Estimated time */
    dsm_time_t ttAdj = _tt0 + toff;

    /* time tag difference between the measured sample time tag 
     * and the adjusted.  This is how much later the measured
     * time tag is from the adjusted time tag. It should be
     * non-negative.
     */
    tdiff = tt - ttAdj;

    /*
     * A negative tdiff violates the assumption that the measured
     * time tags are always late, never early. So in that
     * case we assume that _tt0 is later than it should be.
     */
    if (tdiff < 0) {
        _tt0 += tdiff;
        ttAdj += tdiff;
        tdiff = 0;
    }

    /* smallest latency in this adjustment period. */
    _tdiffminUsec = std::min(tdiff, _tdiffminUsec);

    if (_ntotalPts > _npts * 5) {
        // Could the final _tadjMinUsec will ever be > 0?
        _tadjMinUsec = std::min(tdiff, _tadjMinUsec);
        _tadjMaxUsec = std::max(tdiff, _tadjMaxUsec);
    }

    if (_nDt >= _npts) {
#ifdef DEBUG
        if (debug)
            cerr << "tt=" <<
                nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%5f") <<
                ", _tt0=" <<
                nidas::util::UTime(_tt0).format(true, "%Y %m %d %H:%M:%S.%5f") <<
                ", ttAdj=" <<
                nidas::util::UTime(ttAdj).format(true, "%Y %m %d %H:%M:%S.%5f") <<
                " _tdiffminUsec=" << _tdiffminUsec <<
                " _dtUsecCorr=" << _dtUsecCorr << 
                " _nDt=" << _nDt <<
                endl;
#endif

	dsm_time_t tt0new = ttAdj + _tdiffminUsec;

        // average dt since the beginning of this window
#define USE_DOUBLE_FOR_DTAVG
#ifdef USE_DOUBLE_FOR_DTAVG
        double dtUsec = (double)(tt - _ttNpt0) / _npts;
#else
        int dtUsec = (tt - _ttNpt0 + _npts/2 ) / _npts;
#endif

	/* Adjust tt0 */
	_tt0 = ttAdj = tt0new;

        /* Don't allow backwards times */
        if (ttAdj < _ttAdjLast) ttAdj = _ttAdjLast + 1;

        int n = std::min(_npts * 10, 100U);

        // running average of dtUsec
#ifdef USE_DOUBLE_FOR_DTAVG
        _dtUsecCorr = ::lrint((n - 1) * (double)_dtUsecCorr / n + dtUsec / n);
#else
        // overflow if n-1 is more than 4294. Kept to a max of 100 above
        _dtUsecCorr = ((n - 1) * _dtUsecCorr + dtUsec + n/2) / n;
#endif
        _dtUsecCorr = std::max(_dtUsecCorr, 1U);
        _nDt = 0;
        _ttNpt0 = tt;
	_tdiffminUsec = INT_MAX;

        /* maintain some statistics on the correction */
        _dtUsecCorrMin = std::min(_dtUsecCorrMin, (signed) _dtUsecCorr);
        _dtUsecCorrMax = std::max(_dtUsecCorrMax, _dtUsecCorr);
        _dtUsecCorrSum += dtUsec;
        _nCorrSum++;

    }

#ifdef DEBUG2
    if (debug)
        cerr << std::fixed << std::setprecision(4) <<
            "tt=" << (tt % USECS_PER_DAY) * 1.e-6 <<
            ", ttAdj=" << (ttAdj % USECS_PER_DAY) * 1.e-6 <<
            ", tdiff=" << tdiff * 1.e-6 << 
            ", _tt0=" << (_tt0 % USECS_PER_DAY) * 1.e-6 <<
            ", toff=" << toff * 1.e-6 <<
            ", _nDt=" << _nDt << 
            ", _tdiffmin=" << _tdiffminUsec * 1.e-6 << endl;
#endif

    _ttAdjLast = ttAdj;
    return ttAdj;
}

void TimetagAdjuster::log(int level, const DSMSensor* sensor,
        dsm_sample_id_t id, bool octalLabel)
{
    // getNumPoints() will be at least one if the adjuster
    // has been called.
    if (getNumPoints() > 0) {
        ostringstream ost;
        ost << "ttadjust: " << sensor->getName() << '(' << GET_DSM_ID(id) <<
            ',' << GET_SPS_ID(id);
        if (octalLabel) {
            int label = id - sensor->getId();
            ost << " lab=" << setw(4) << setfill('0') << std::oct << label;
        }
        ost << ')';
        nidas::util::Logger::getInstance()->log(level, __FILE__, __PRETTY_FUNCTION__, __LINE__,
            "%s: max late:%7.4fs, dt min,max:%9.5f,%9.5f, rate cfg,obs,diff:%6.2f,%9.5f,%6.2f, nAdj > dt: %u, #back: %u, #gap: %u, maxgap:%7.2f, #points: %u",
            ost.str().c_str(),
            getAdjMax(), getDtMin(), getDtMax(),
            getRate(), 1.0 / getDtAvg(), getRate()- 1.0 / getDtAvg(),
            getNumLargeAdjust(), getNumBackwards(), getNumGaps(), getMaxGap(),
            getNumPoints());
    }
}
