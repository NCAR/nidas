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

TimetagAdjuster::TimetagAdjuster(double rate, float adjustSecs,
    float sampleGap):
    _tt0(0), _tlast(0),
    _dtUsec((unsigned int) ::lrint(USECS_PER_SEC / rate)),
    _dtUsecCorr(_dtUsec),
    // keep adjustSecs to less than 1/2 hour. It should be typically 
    // less than 10 seconds.
    _adjustUsec(::lrint(std::min(adjustSecs,1800.0f) * USECS_PER_SEC)),
    _nDt(0),
    _npts(_adjustUsec / _dtUsec),
    _tdiffminUsec(INT_MAX),
    _dtGapUsec(::lrint(_dtUsec * sampleGap)),
    _nLargeAdjust(0),
    _dtUsecCorrMin(INT_MAX), _dtUsecCorrMax(0),
    _dtUsecCorrSum(0.0), _nCorrSum(0),
    _nBack(0), _nGap(0),
    _tadjMinUsec(INT_MAX), _tadjMaxUsec(INT_MIN),
    _ntotalPts(0)
{
}

dsm_time_t TimetagAdjuster::adjust(dsm_time_t tt)
{

    if (_adjustUsec <= 0) return tt;

    int tdiff = tt - _tlast;
    _ntotalPts++;
#ifdef DEBUG
    cerr << nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%5f") <<
        " tdiff=" << tdiff <<
        " _tadjMaxUsec=" << _tadjMaxUsec << endl;
#endif
    if (tdiff < 0) {
        _nBack++;
        return tt;
    }
    _tlast = tt;
    if (tdiff > _dtGapUsec) {
        _tt0 = tt;
        _nDt = 0;
        _tdiffminUsec = INT_MAX;
        /* 10 points initially */
        _npts = 10;
        if (_ntotalPts == 1) _dtUsecCorr = _dtUsec;
        _nGap++;
        return tt;
    }
    _nDt++;

    /* Expected time from _tt0, as an integral number of dt's.
     * The max time diff in toff (32 bit unsigned int) is 2^32/10^6 = 
     * 4294 seconds, over an hour. So 32 bit unsigned int
     * should be large enough.  _adjustUsec is limited to 1800 sec
     * and so _nDt will be less than 1800 / _dtUsecCorr.
     */
    unsigned int toff = _nDt * _dtUsecCorr;

    /* Expected time */
    dsm_time_t tt_est = _tt0 + toff;

    /* time tag difference between actual and expected, the
     * correction that is being applied */
    tdiff = tt - tt_est;

    if (tdiff > (signed)_dtUsec) {
        _nLargeAdjust++;

#ifdef DEBUG
        cerr << nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%5f") <<
            " _npts=" << _npts << ", _nDt=" << _nDt <<
            ", _dtUsecCorr=" << _dtUsecCorr <<
            ", tdiff=" << tdiff << endl;
#endif
        // reduce the stats period by 10% since correction was large
        _npts = std::max(10U, (unsigned int)::lrint(_npts * 0.9));
    }

    /* minimum difference in this adjustment period. */
    _tdiffminUsec = std::min(tdiff, _tdiffminUsec);

    _tadjMinUsec = std::min(tdiff, _tadjMinUsec);
    _tadjMaxUsec = std::max(tdiff, _tadjMaxUsec);

    if (_nDt >= _npts) {
#ifdef DEBUG
        cerr << nidas::util::UTime(tt).format(true, "%Y %m %d %H:%M:%S.%5f") <<
            " _dtUsecCorr=" << _dtUsecCorr << 
            " _nDt=" << _nDt << 
            " _tdiffminUsec=" << _tdiffminUsec <<  endl;
#endif

	/* Adjust tt0 */
	_tt0 = tt_est + _tdiffminUsec;
        tt_est = _tt0;

        /* Tweak the sampling dt using the minimum difference between
         * the measured and the estimated time tags.
         * If the minimum difference is positive, then the measured
         * timetags are later than the estimated, and the estimated
         * dt (_duUsecCorr) will be increased.
         * The adjustment to _dtUsecCorr is gradual, the difference
         * is divided ty _nDt to avoid wild changes.
         */

        _dtUsecCorr += (int) ::lrint(_tdiffminUsec / _nDt);
        // at least 10 points
        _npts = std::max(10U, (unsigned int)::lrint(_adjustUsec / _dtUsecCorr));
        _nDt = 0;
	_tdiffminUsec = INT_MAX;

        /* maintain some statistics on the correction */
        _dtUsecCorrMin = std::min(_dtUsecCorrMin, _dtUsecCorr);
        _dtUsecCorrMax = std::max(_dtUsecCorrMax, _dtUsecCorr);
        _dtUsecCorrSum += _dtUsecCorr;
        _nCorrSum++;

    }

#ifdef DEBUG
    cerr << std::fixed << std::setprecision(4) <<
        "tt=" << (tt % USECS_PER_DAY) * 1.e-6 <<
        ", tt_est=" << (tt_est % USECS_PER_DAY) * 1.e-6 <<
        ", tdiff=" << tdiff * 1.e-6 << 
        ", _tt0=" << (_tt0 % USECS_PER_DAY) * 1.e-6 <<
        ", toff=" << toff * 1.e-6 <<
        ", _nDt=" << _nDt << 
        ", _tdiffmin=" << _tdiffminUsec * 1.e-6 << endl;
#endif

    return tt_est;
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
            "%s: min,max: %8.4f,%8.4f, dt min,max: %8.4f,%8.4f, rate cfg,obs,diff: %6.2f,%9.5f,%6.2f, nAdj > dt: %u, #back: %u, #gap: %u, #points: %u",
            ost.str().c_str(),
            getAdjMin(), getAdjMax(),
            getDtMin(), getDtMax(),
            getRate(), 1.0 / getDtAvg(), getRate()- 1.0 / getDtAvg(),
            getNumLargeAdjust(), getNumBackwards(), getNumGaps(), getNumPoints());
    }
}
