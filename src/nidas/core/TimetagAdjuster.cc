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

#include <cmath>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace std;

TimetagAdjuster::TimetagAdjuster(double rate, float adjustSecs):
    _tt0(LONG_LONG_MIN), _tlast(LONG_LONG_MIN),
    _dtUsec((unsigned int) ::rint(USECS_PER_SEC / rate)),
    _dtUsecActual((unsigned int) ::rint(USECS_PER_SEC / rate)),
    _nptsCalc((unsigned int)(adjustSecs * USECS_PER_SEC) / _dtUsec),
    _nDt(0),
    _nmin(0),
    _nptsMin(0),
    _tdiffminUsec(INT_MAX),
    _dtGapUsec(19 * _dtUsec / 10)
{
}

dsm_time_t TimetagAdjuster::adjust(dsm_time_t tt)
{
    /*
     * Reset on a data gap, or backwards, screwy time.
     * A gap is defined as a difference of 1.9 * dt between
     * raw samples.
     */
    int tdiff = tt - _tlast;
    _tlast = tt;
    if (tdiff < 0 || tdiff > _dtGapUsec) {
        _tt0 = tt;
        _nDt = 1;
        _tdiffminUsec = INT_MAX;
        _nmin = 0;
        _nptsMin = 10;  /* 10 points initially */
        _dtUsecActual = _dtUsec;
        return tt;
    }

    /* Expected time from _tt0, assuming a fixed delta-T.
     * max value in toff (32 bit int) is 4.2*10^9 usecs, or
     * 4200 seconds, which is over an hour. So 32 bit int
     * should be large enough */
    unsigned int toff = _nDt * _dtUsecActual;

    /* Expected time */
    dsm_time_t tt_est = _tt0 + toff;

    /* time tag difference between actual and expected */
    tdiff = tt - tt_est;

    _nmin++;

    /* minimum difference in this adjustment period. */
    _tdiffminUsec = std::min(tdiff, _tdiffminUsec);

    if (_nmin == _nptsMin) {
	/* Adjust tt0 */
	_tt0 = tt_est + _tdiffminUsec;
        /* tweak the sampling delta-T from the observed times */
        _dtUsecActual += _tdiffminUsec / _nDt;
        _nDt = 0;
        toff = 0;
	_nmin = 0;
	_tdiffminUsec = INT_MAX;
	_nptsMin = _nptsCalc;
    }
    _nDt++;

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

    return _tt0 + toff;
}
