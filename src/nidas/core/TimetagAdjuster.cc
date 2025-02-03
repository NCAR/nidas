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

#include "TimetagAdjuster.h"
#include "SampleTracer.h"
#include <nidas/util/Logger.h>

#include <cmath>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace std;

TimetagAdjuster::TimetagAdjuster(dsm_sample_id_t id, double rate):
    _tt0(LONG_LONG_MIN), _tlast(LONG_LONG_MIN), _ttnDt0(LONG_LONG_MIN),
    _ttAdjLast(LONG_LONG_MIN),
    _maxGap(0),
    _dtUsecCorrSum(0.0),
    _id(id),
    _dtUsec((int) ::lrint(USECS_PER_SEC / rate)),
    _dtUsecCorr(_dtUsec),
    _nDt(0),
    _npts(std::max(5U, 1 * USECS_PER_SEC / _dtUsec)),
    _tdiffminUsec(INT_MAX),
    _dtUsecCorrMin(INT_MAX), _dtUsecCorrMax(0),
    _nCorrSum(0),
    _nBack(0),
    _tadjMinUsec(INT_MAX), _tadjMaxUsec(0),
    _ntotalPts(0),
    _dtResultMin(INT_MAX),
    _dtResultMax(0),
    _tdiffLast(0),
    _nNegTdiff(0),
    _nBigTdiff(0),
    _nworsen(0),
    _nimprove(0),
    _nSamp5Min(5 * 60 * USECS_PER_SEC / _dtUsec)
{
    /*
     * _npts = max(5, 1 * USECS_PER_SEC / dtUsec)
     * rate     _npts
     * 1        5
     * 2        5
     * 10       10
     * 50       50
     * 100      100
     */
}

dsm_time_t TimetagAdjuster::adjust(dsm_time_t tt)
{
    /*
     * To enable the SampleTracer for a given sample id, add these arguments
     * to data_dump or any program that processes samples:
     *  --log enable,level=verbose,function=TimetagAdjuster::adjust
     *  --logparam trace_samples=20,141
     *  --logfields level,message
     */
    static SampleTracer stracer(LOG_VERBOSE);

    _ntotalPts++;

    if (_ntotalPts == 1) {
        _ttnDt0 = _tt0 = _tlast = _ttAdjLast = tt;
        return tt;
    }

    if (tt < _tlast) {
        _nBack++;
        if (_ttAdjLast > 0) {
            int tdiff = tt - _ttAdjLast;
            _dtResultMin = std::min(_dtResultMin, tdiff);
            _dtResultMax = std::max(_dtResultMax, tdiff);
        }
        _ttAdjLast = tt;
        return tt;
    }

    // max dt in a 32 bit int: 2^32/1.e6 = 4294 seconds
    // so use 64 bit, to avoid overflow with large gaps
    long long gap = tt - _tlast;    
    _tlast = tt;
    _maxGap = std::max(_maxGap, gap);

    if (gap > BIG_GAP_SECONDS * USECS_PER_SEC) {
        // big gap, restart
        _ttnDt0 = _tt0 = tt;
        _nDt = 0;
        _tdiffminUsec = INT_MAX;
        if (_ttAdjLast > 0) {
            int tdiff = tt - _ttAdjLast;
            _dtResultMin = std::min(_dtResultMin, tdiff);
            _dtResultMax = std::max(_dtResultMax, tdiff);
        }
        _ttAdjLast = tt;
        return tt;
    }
    _nDt++;

    /* Offset from _tt0, an integral number of dt's. */
    long long toff = _nDt * (long long)_dtUsecCorr;

    /* Estimated, adjusted time */
    dsm_time_t ttAdj = _tt0 + toff;

    /* difference between the measured sample time tag 
     * and the estimated.  This is how much later the measured
     * time tag is from the adjusted time tag. It should be
     * non-negative.
     */
    int tdiff = tt - ttAdj;     // signed
    int tdiffUncorr = tdiff;

    /*
     * A negative tdiff violates the assumption that the measured
     * time tags are always late, never early. So here, _tt0 is too late
     * and must be adjusted earlier. This happens at startup before
     * we have good estimates for _tdiffminUsec, if the system is
     * responding more quickly in this set of _npts that it was in
     * the previous period, or if the actual sample dt is less than
     * _dtUsecCorr.
     */
    if (tdiff < 0) {
        if (_ntotalPts > _npts && -tdiff > (signed)_dtUsecCorr / 2) {
            _nNegTdiff++;
            DLOG(("ttadjust: tdiff < -dt/2: %s, id=%d,%d, tdiff=%6.2f, dt=%6.2f, #neg=%u",
                nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                GET_DSM_ID(_id), GET_SPS_ID(_id),
                (double)tdiff/USECS_PER_SEC,
                (double)_dtUsec/USECS_PER_SEC, _nNegTdiff));
        }
        _tt0 += tdiff;
        ttAdj += tdiff;
        // slight chance of adjusted time being backwards
        if (ttAdj < _ttAdjLast) ttAdj = _ttAdjLast + 1;
        tdiff = 0;
    }

    int tdiffdiff = tdiff - _tdiffLast;
    _tdiffLast = tdiff;

    if (tdiffdiff < 0) {
        // tdiff is improving
        _nimprove++;
        _nworsen = 0;
    }
    else if (tdiffdiff >= 0) {
        // if more than two non-negative tdiffdiffs in a row, then we're
        // not improving.
        _nworsen++;
        if (_nworsen > 1) _nimprove = 0;
    }

    /* smallest latency in this adjustment period. */
    _tdiffminUsec = std::min(tdiff, _tdiffminUsec);

    if (_ntotalPts > _npts * 5) {
        // Keep track of min and max tdiff after a few periods
        _tadjMinUsec = std::min(tdiff, _tadjMinUsec);
        _tadjMaxUsec = std::max((unsigned) tdiff, _tadjMaxUsec);
    }

    slog(stracer, "HR ", tt, toff, tdiff, tdiffUncorr);

    if ( _nDt >= _npts && _nimprove == 0) {

        slog(stracer, "LR ", ttAdj);

        if (_tdiffminUsec > (signed)_dtUsecCorr / 2) {
            _nBigTdiff++;
            DLOG(("ttadjust: tdiff > dt/2: %s, id=%d,%d, tdiff=%6.2f, dt=%6.2f, #big=%u",
                nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                GET_DSM_ID(_id), GET_SPS_ID(_id),
                (double)_tdiffminUsec/USECS_PER_SEC,
                (double)_dtUsec/USECS_PER_SEC, _nBigTdiff));
        }

	/* Adjust tt0 */
        _tt0 = ttAdj + _tdiffminUsec;

        ttAdj = _tt0;

        /* Don't allow backwards times */
        if (ttAdj < _ttAdjLast) ttAdj = _ttAdjLast + 1;

        // average dt since the beginning of this window
        int dtUsec = (tt - _ttnDt0 + _nDt/2 ) / _nDt;
        _ttnDt0 = tt;

        // 5 minute running average of dtUsec
        unsigned int n = min(_nSamp5Min, _ntotalPts);
        _dtUsecCorr = ::rint(((double)_dtUsecCorr * (n - _nDt) +
            (double)dtUsec * _nDt) / n);
        _dtUsecCorr = std::max(_dtUsecCorr, 1U);

        _nDt = 0;
	_tdiffminUsec = INT_MAX;

        /* maintain some statistics on the correction */
        _dtUsecCorrMin = std::min(_dtUsecCorrMin, (signed) _dtUsecCorr);
        _dtUsecCorrMax = std::max(_dtUsecCorrMax, _dtUsecCorr);
        _dtUsecCorrSum += dtUsec;
        _nCorrSum++;
    }

    if (_ttAdjLast > 0) {
        tdiff = ttAdj - _ttAdjLast;
        _dtResultMin = std::min(_dtResultMin, tdiff);
        _dtResultMax = std::max(_dtResultMax, tdiff);
    }
    _ttAdjLast = ttAdj;

    return ttAdj;
}

void TimetagAdjuster::slog(SampleTracer& stracer,
    const string& msg, dsm_time_t tt, long long toff, int tdiff,
    int tdiffUncorr)
{
    if (stracer.active(_id))
    {
        stracer.msg(tt, _id, msg) <<
            ", toff tdiff tdiffUncorr tdiffmin nDt: " <<
            setw(6) << 
            (double)toff / USECS_PER_SEC << " " <<
            (double)tdiff / USECS_PER_SEC <<  " " <<
            (double)tdiffUncorr / USECS_PER_SEC <<  " " <<
            (double)_tdiffminUsec / USECS_PER_SEC << " " <<
            _nDt <<
            nidas::util::endlog;
    }
}

void TimetagAdjuster::slog(SampleTracer& stracer,
    const string& msg, dsm_time_t tt)
{
    if (stracer.active(_id))
    {
        stracer.msg(tt, _id, msg) <<
            ", _ttnDt0=" <<
            stracer.format_time(_ttnDt0,"%H:%M:%S.%4f") <<
            ", tdiffmin=" << (double)_tdiffminUsec / USECS_PER_SEC <<
            ", dtCorr=" << (double)_dtUsecCorr / USECS_PER_SEC <<
            nidas::util::endlog;
    }
}

/*
 * Used by sensor classes to log their ttadjust results on shutdown.
 */
void TimetagAdjuster::log(int level, const DSMSensor* sensor,
        bool octalLabel)
{
    // getNumPoints() will be at least one if the adjuster
    // has been called.
    if (getNumPoints() > 0) {
        ostringstream ost;
        ost << "ttadjust: " << sensor->getName() << '(' << GET_DSM_ID(_id) <<
            ',' << GET_SPS_ID(_id);
        if (octalLabel) {
            int label = _id - sensor->getId();
            ost << " lab=" << setw(4) << setfill('0') << std::oct << label;
        }
        ost << ')';
        nidas::util::Logger::getInstance()->log(level,
            __FILE__, __PRETTY_FUNCTION__, __LINE__,
            "%s: max late:%7.4fs, dt min,max:%9.5f,%9.5f, outdt min,max:%6.2f,%6.2f, rate cfg,obs,diff:%6.2f,%9.5f,%6.2f, maxgap:%7.2f, #neg: %u, #pos: %u, #tot: %u",
            ost.str().c_str(),
            getAdjMax(), getDtMin(), getDtMax(), getMinResultDt(), getMaxResultDt(),
            getRate(), 1.0 / getDtAvg(), getRate()- 1.0 / getDtAvg(),
            getMaxGap(), _nNegTdiff, _nBigTdiff, getNumPoints());
    }
}
