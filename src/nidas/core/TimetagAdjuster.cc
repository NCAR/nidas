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

TimetagAdjuster::Parameters::Parameters(double rate):
    BIG_GAP_USEC(5 * USECS_PER_SEC),
    N_ADJ(std::max(5, (int)(1.0 * rate))),
    N_AVG((int) (1 * 60 * rate)),
    DT_LIMIT_FRACTION(0.3),
    MAX_CONSEC_BAD_DT(5),
    LATENCY_WARN_DT(0.5),
    LATENCY_WORSEN_MAX(3),
    MAX_CONSEC_NEG_LATENCY(5)
{
}

TimetagAdjuster::TimetagAdjuster(dsm_sample_id_t id, double rate):
    _params(rate),
    _id(id),
    _dtUsecConfig(USECS_PER_SEC / rate),
    _ntotalPts(0),
    _ttlast(LONG_LONG_MIN),
    _nBack(0),
    _ngap(0),
    _maxGap(0),
    _tt0(LONG_LONG_MIN),
    _nDt(0),
    _ttnDt0(LONG_LONG_MIN),
    _dtUsecAvg(_dtUsecConfig),
    _dtUsecAvgSum(0.0),
    _nDtAvgSum(0),
    _dtUsecLowLimit(_dtUsecConfig * (1.0 - _params.DT_LIMIT_FRACTION)),
    _dtUsecHighLimit(_dtUsecConfig * (1.0 + _params.DT_LIMIT_FRACTION)),
    _nDtLow(0), _nDtHigh(0),
    _maxConsecLow(0), _maxConsecHigh(0),
    _dtUsecAvgMin(1.e32), _dtUsecAvgMax(0.0),
    _dtResultMin(INT_MAX), _dtResultMax(0),
    _latencyAdjUsec(INT_MAX),
    _nLargeLatency(0),
    _latencyLast(0),
    _nNegLatency(0),
    _ttAdjLast(LONG_LONG_MIN),
    _latencyMinUsec(INT_MAX),
    _latencyMaxUsec(0),
    _nConsecNegLatency(0)
{
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
        _ttnDt0 = _tt0 = _ttlast = _ttAdjLast = tt;
        sloghr(stracer, "HR ", tt, 0, 0);
        return tt;
    }

    if (tt < _ttlast) {
        cerr << nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f") << " BACKWARDS!!!!!!!" << endl;
        if (_nBack++ < 5)
            WLOG(("ttadjust backwards time %s, id=%d,%d: back=%8.4f sec",
                nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                GET_DSM_ID(_id), GET_SPS_ID(_id), (double)(tt - _ttlast) / USECS_PER_SEC));
#ifdef PUNT_ON_BACKWARDS
        if (_ttAdjLast > 0) {
            int dt = tt - _ttAdjLast;
            _dtResultMin = std::min(_dtResultMin, dt);
            _dtResultMax = std::max(_dtResultMax, dt);
        }
        _ttAdjLast = tt;
        sloghr(stracer, "HR ", tt, 0, 0);
        return tt;
#endif
    }

    // max dt in a 32 bit int: 2^32/1.e6 = 4294 seconds
    // so use 64 bit, to avoid overflow with large gaps
    long long gap = tt - _ttlast;    
    _ttlast = tt;
    _maxGap = std::max(_maxGap, gap);

    if (gap > _params.BIG_GAP_USEC) {
        if (_ngap < 5)
            WLOG(("ttadjust large gap %s, id=%d,%d: gap=%8.4f > %8.4f",
                nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                GET_DSM_ID(_id), GET_SPS_ID(_id),
                (double)gap/USECS_PER_SEC,
                (double)_params.BIG_GAP_USEC/ USECS_PER_SEC));

        // big gap, restart
        _ngap++;
        _ttnDt0 = _tt0 = tt;
        _nDt = 0;
        _latencyAdjUsec = INT_MAX;
        if (_ttAdjLast > 0) {
            int dt = tt - _ttAdjLast;
            _dtResultMin = std::min(_dtResultMin, dt);
            _dtResultMax = std::max(_dtResultMax, dt);
        }
        _ttAdjLast = tt;
        sloghr(stracer, "HR ", tt, 0, 0);
        return tt;  // no correction
    }
    _nDt++;

    /* Offset from _tt0, an integral number of dt's. */
    double toff = _nDt * _dtUsecAvg;

    /* Adjusted time */
    dsm_time_t ttAdj = ::llrint(_tt0 + toff);

    /* Estimated latency: the difference between the measured
     * sample time tag and the adjusted time tag.
     * Ideally it should be >= 0.
     */
    int latency = tt - ttAdj;

    /*
     * A negative latency violates the assumption that with a good
     * DSM clock, the measured time tags are always late, never early.
     * So here it means that _tt0 is too late and must be adjusted
     * earlier. This can happen:
     * 1. at startup before * we have good estimates for _tt0, and
     *  _dtUsecAvg, and _latencyAdjUsec, or
     * 2.  if the system is responding more quickly in this set of
     *  N_ADJ samples that it was in the previous set, or
     * 3. if the actual sample dt is less than _dtUsecAvg,
     *    which can happen due to skipped samples.
     *    
     */
    if (latency < 0) {
        _nNegLatency++;
        _nConsecNegLatency++;
        if (_nConsecNegLatency > _params.MAX_CONSEC_NEG_LATENCY) {
            WLOG(("ttadjust neg latency %s, id=%d,%d: latency=%8.4f, dt=%8.4f, #consec=%u, resetting",
                nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                GET_DSM_ID(_id), GET_SPS_ID(_id),
                (double)latency/USECS_PER_SEC,
                (double)_dtUsecAvg/USECS_PER_SEC, _nConsecNegLatency));
            _ttnDt0 = _tt0 = tt;
            _nDt = 0;
            _dtUsecAvg = _dtUsecConfig;
            _latencyAdjUsec = INT_MAX;
            _ttAdjLast = tt;
            sloghr(stracer, "HR ", tt, 0, latency);
            return tt;  // no correction
        }

        // report if it is more than 0.5 dt negative
        if (_ntotalPts > _params.N_ADJ &&
            -latency > _dtUsecAvg * _params.LATENCY_WARN_DT) {
            WLOG(("ttadjust neg latency %s, id=%d,%d: latency=%8.4f, dt=%8.4f, #neg=%u",
                nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                GET_DSM_ID(_id), GET_SPS_ID(_id),
                (double)latency/USECS_PER_SEC,
                (double)_dtUsecAvg/USECS_PER_SEC, _nNegLatency));
        }
        _tt0 += latency;      // shift backwards
        ttAdj += latency;
        // slight chance of adjusted time being backwards
        if (ttAdj < _ttAdjLast) ttAdj = _ttAdjLast + 1;

        if (_ttAdjLast > 0) {
            int dt = ttAdj - _ttAdjLast;
            _dtResultMin = std::min(_dtResultMin, dt);
            _dtResultMax = std::max(_dtResultMax, dt);
        }

        _latencyAdjUsec = 0;
        _latencyLast = latency;
        _ttAdjLast = ttAdj;
        _latencyAdjUsec = std::min(latency, _latencyAdjUsec);
        sloghr(stracer, "HR neg latency ", tt, tt - ttAdj, latency);
        return ttAdj;
    }

    _nConsecNegLatency = 0;

    int latencyDiff = latency - _latencyLast;
    _latencyLast = latency;

    /* smallest latency in this adjustment period. */
    _latencyAdjUsec = std::min(latency, _latencyAdjUsec);

    // end of adjustment window and latency has ceased improving.
    if ( _nDt >= _params.N_ADJ && (latencyDiff >= 0 || _nDt > _params.N_ADJ * 2)) {

        // average delta-T of last _params.N_ADJ samples
        int dtUsec = (tt - _ttnDt0 + _nDt/2 ) / _nDt;

        // low rate debug msg
        sloglr(stracer, "LR ", tt, tt-ttAdj, dtUsec);

        // screen it
        bool skipdt = screenDt(tt, dtUsec);

        if (! skipdt) {
            // running average of delta-T averages
            unsigned int n = min(_params.N_AVG, _ntotalPts);
            _dtUsecAvg = (_dtUsecAvg * (n - _nDt) + dtUsec * _nDt) / n;
        }

        _ttnDt0 = tt;
        _nDt = 0;

        // Screen large latency
        if (_latencyAdjUsec > _dtUsecAvg * _params.LATENCY_WARN_DT) {
            _nLargeLatency++;
            if (_nLargeLatency < 5) 
                WLOG(("ttadjust large latency %s id=%d,%d: latency > dt X %4.1f: latency=%8.4f sec, dt=%8.4f, #large=%u",
                    nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                    GET_DSM_ID(_id), GET_SPS_ID(_id),
                    _params.LATENCY_WARN_DT,
                    (double)_latencyAdjUsec / USECS_PER_SEC,
                    _dtUsecAvg/USECS_PER_SEC, _nLargeLatency));
            _tt0 = tt;
        }
        else
            _tt0 = ttAdj + _latencyAdjUsec;

        /* maintain some statistics on the correction */
        _latencyMinUsec = std::min(_latencyAdjUsec, _latencyMinUsec);
        _latencyMaxUsec = std::max(_latencyAdjUsec, _latencyMaxUsec);
        _dtUsecAvgMin = std::min(_dtUsecAvgMin, _dtUsecAvg);
        _dtUsecAvgMax = std::max(_dtUsecAvgMax, _dtUsecAvg);
        _dtUsecAvgSum += dtUsec;
        _nDtAvgSum++;

        _latencyAdjUsec = INT_MAX; // over _params.N_ADJ samples
    }

    /* Don't allow backwards times */
    if (ttAdj < _ttAdjLast) ttAdj = _ttAdjLast + 1;

    if (_ttAdjLast > 0) {
        int dt = ttAdj - _ttAdjLast;
        _dtResultMin = std::min(_dtResultMin, dt);
        _dtResultMax = std::max(_dtResultMax, dt);
    }

    _ttAdjLast = ttAdj;

    // high rate debug msg
    sloghr(stracer, "HR ", tt, tt - ttAdj, latency);
    return ttAdj;
}

bool TimetagAdjuster::screenDt(dsm_time_t tt, double dtUsec)
{
    bool skipdt = false;
    if (dtUsec < _dtUsecLowLimit) {
        _maxConsecHigh = 0;
        if (++_maxConsecLow > _params.MAX_CONSEC_BAD_DT) {
            WLOG(("ttadjust: %s, id=%d,%d, avg dt=%8.4f sec is below %8.4f for %d consecutive times, might as well use it",
            nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
            GET_DSM_ID(_id), GET_SPS_ID(_id),
            dtUsec / USECS_PER_SEC, _dtUsecLowLimit / USECS_PER_SEC,
            _maxConsecLow));
            skipdt = false;
            // adjust limits
            _dtUsecAvg = _dtUsecConfig = dtUsec;
            _dtUsecLowLimit = _dtUsecConfig * (1.0 - _params.DT_LIMIT_FRACTION);
            _dtUsecHighLimit = _dtUsecConfig * (1.0 + _params.DT_LIMIT_FRACTION);
            _maxConsecLow = 0;
        }
        else if (!(_nDtLow % 100)) {
            WLOG(("ttadjust: %s, id=%d,%d, avg dt=%8.4f sec is below %8.4f, skipping",
            nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
            GET_DSM_ID(_id), GET_SPS_ID(_id),
            dtUsec / USECS_PER_SEC, _dtUsecLowLimit / USECS_PER_SEC));
            skipdt = true;
        }
        _nDtLow++;
    }
    else if (dtUsec > _dtUsecHighLimit) {
        _maxConsecLow = 0;
        if (++_maxConsecHigh > _params.MAX_CONSEC_BAD_DT) {
            WLOG(("ttadjust: %s, id=%d,%d, avg dt=%8.4f sec is above %8.4f, _nDt=%d, _ttnDt0=%s, for %d consecutive times, might as well use it",
            nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
            GET_DSM_ID(_id), GET_SPS_ID(_id),
            dtUsec / USECS_PER_SEC, _dtUsecHighLimit / USECS_PER_SEC,
            _nDt,
            nidas::util::UTime(_ttnDt0).format(true, "%H:%M:%S.%3f").c_str(),
            _maxConsecHigh));
            skipdt = false;
            // adjust limits
            _dtUsecAvg = _dtUsecConfig = dtUsec;
            _dtUsecLowLimit = _dtUsecConfig * (1.0 - _params.DT_LIMIT_FRACTION);
            _dtUsecHighLimit = _dtUsecConfig * (1.0 + _params.DT_LIMIT_FRACTION);
            _maxConsecHigh = 0;
        }
        else if (!(_nDtHigh % 100)) {
            WLOG(("ttadjust: %s, id=%d,%d, avg dt=%8.4f sec is above %8.4f, _nDt=%d, _ttnDt0=%s, skipping",
            nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
            GET_DSM_ID(_id), GET_SPS_ID(_id),
            dtUsec / USECS_PER_SEC, _dtUsecHighLimit / USECS_PER_SEC,
            _nDt,
            nidas::util::UTime(_ttnDt0).format(true, "%H:%M:%S.%3f").c_str()));
            skipdt = true;
        }
        _nDtHigh++;
    }
    else {
        _maxConsecLow = 0;
        _maxConsecHigh = 0;
    }
    return skipdt;
}

void TimetagAdjuster::sloghr(SampleTracer& stracer,
    const string& msg, dsm_time_t tt, int dtAdj, int latency)
{
    if (stracer.active(_id))
    {
        stracer.msg(tt, _id, msg) <<
            " " << setw(8) << 
            (double)dtAdj / USECS_PER_SEC << " " <<
            (double)latency / USECS_PER_SEC << ' ' <<
            _nDt <<
            nidas::util::endlog;
    }
}

void TimetagAdjuster::sloglr(SampleTracer& stracer,
    const string& msg, dsm_time_t tt, int dtAdj, double dtUsec)
{
    if (stracer.active(_id))
    {
        stracer.msg(tt, _id, msg) <<
            ", dtAdj=" << setw(8) << 
            (double)dtAdj / USECS_PER_SEC << " " <<
            ", _ttnDt0=" <<
            stracer.format_time(_ttnDt0,"%H:%M:%S.%4f") <<
            ", latencyAdj=" << (double) _latencyAdjUsec / USECS_PER_SEC <<
            ", dt=" << dtUsec / USECS_PER_SEC <<
            ", _nDt=" << _nDt <<
            ", dtUsecAvg=" << _dtUsecAvg / USECS_PER_SEC <<
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
            "%s: #back=%u, #gap=%u. maxgap=%8.4f sec, \
rate cfg,obs,diff:%6.2f,%9.5f,%8.4f, \
avgDt min,max:%8.4f, %8.4f, #lowDt=%u, #highDt=%u, \
outDt min,max:%8.4f,%8.4f, \
latency min,max:%9.5f,%9.5f, \
#large latency=%u, \
#neg latency: %u, #samples: %u",
            ost.str().c_str(),
            _nBack, _ngap,
            (double)_maxGap / USECS_PER_SEC,
            getRate(), getAvgRate(), getRate() - getAvgRate(),
            getDtMin(), getDtMax(), _nDtLow, _nDtHigh, 
            getMinResultDt(), getMaxResultDt(),
            getLatencyMin(), getLatencyMax(),
            _nLargeLatency, _nNegLatency,
            getNumPoints());
    }
}
