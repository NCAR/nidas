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

#include <assert.h>

#include <iostream>

#include "A2DConverter.h"
#include "CalFile.h"

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

A2DConverter::A2DConverter(int nchan, int ncoefs):
    _maxNumChannels(nchan),
    _numConfigChannels(0),
    _ncoefs(ncoefs),
    _gain(new int[nchan]), _bipolar(new int[nchan])
{
    for (int i = 0; i < nchan; i++) {
        _gain[i] = 0;
        _bipolar[i] = -1;       // -1 is an unuusd channel
    }
}

A2DConverter::~A2DConverter()
{
    delete [] _gain;
    delete [] _bipolar;
}

#ifdef DEFINE_A2DC_COPY_ASSIGN
A2DConverter::A2DConverter(const A2DConverter& x):
    _maxNumChannels(x._maxNumChannels), _ncoefs(x._ncoefs),
    _gain(new int[_maxNumChannels]), _bipolar(new int[_maxNumChannels])
{
    for (int i = 0; i < _maxNumChannels; i++) {
        _gain[i] = x._gain[i];
        _bipolar[i] = x._bipolar[i];
    }
}

A2DConverter& A2DConverter::operator =(const A2DConverter& x)
{
    if (&x == this) return *this;
    if (_maxNumChannels != x._maxNumChannels) {
        delete [] _gain; _gain = new int[x._maxNumChannels];
        delete [] _bipolar; _bipolar = new int[x._maxNumChannels];
        _maxNumChannels = x._maxNumChannels;
    }
    for (int i = 0; i < _maxNumChannels; i++) {
        _gain[i] = x._gain[i];
        _bipolar[i] = x._bipolar[i];
    }
    return *this;.
}
#endif

int A2DConverter::getGain(int ichan) const
{
    if (ichan < 0 || ichan >= _maxNumChannels) return -1;
    return _gain[ichan];
}

void A2DConverter::setGain(int ichan, int val)
{
    if (ichan >= 0 && ichan < _maxNumChannels) _gain[ichan] = val;
    if (val > 0)
        _numConfigChannels = ichan + 1;
}

int A2DConverter::getBipolar(int ichan) const
{
    if (ichan < 0 || ichan >= _maxNumChannels) return -1;
    return _bipolar[ichan];
}

void A2DConverter::setBipolar(int ichan, int val)
{
    if (ichan >= 0 && ichan < _maxNumChannels) _bipolar[ichan] = val;
}

LinearA2DConverter::LinearA2DConverter(int nchan):
    A2DConverter(nchan, 2),
    _b(new float[nchan]),
    _mx(new float[nchan])
{
    for (int i = 0; i < nchan; i++) {
        _b[i] = 0.0;
        _mx[i] = 1.0;
    }
}

LinearA2DConverter::~LinearA2DConverter()
{
    delete [] _b;
    delete [] _mx;
}

#ifdef DEFINE_A2DC_COPY_ASSIGN
LinearA2DConverter::LinearA2DConverter(const LinearA2DConverter& x):
    A2DConverter(x._maxNumChannels, 2),
    _b(new float[_maxNumChannels]),
    _mx(new float[_maxNumChannels])
{
    for (int i = 0; i < _maxNumChannels; i++) {
        _b[i] = x._b[i];
        _mx[i] = x._mx[i];
    }
}

LinearA2DConverter& LinearA2DConverter::operator=(const LinearA2DConverter& x)
{
    if (&x == this) return *this;
    if (_maxNumChannels != x._maxNumChannels) {
        delete [] _b; _b = new float[x._maxNumChannels];
        delete [] _mx; _mx = new float[x._maxNumChannels];
        _maxNumChannels = x._maxNumChannels;
    }

    for (int i = 0; i < _maxNumChannels; i++) {
        _b[i] = x._b[i];
        _mx[i] = x._mx[i];
    }
    return *this;
}
#endif

float LinearA2DConverter::convert(int ichan, float counts) const
{
    assert(ichan >= 0 && ichan < _maxNumChannels);
    return _b[ichan] + _mx[ichan] * counts;
}

void LinearA2DConverter::set(int ichan, const float* d, int nd)
{
    assert(ichan >= 0 && ichan < _maxNumChannels);
    assert(nd == 2);
    _b[ichan] = d[0];
    _mx[ichan] = d[1];
}

void LinearA2DConverter::get(int ichan, float* d, int nd) const
{
    assert(ichan >= 0 && ichan < _maxNumChannels);
    assert(nd >= 2);
    d[0] = _b[ichan];
    d[1] = _mx[ichan];
    for (int i = 2; i < nd; i++) d[i] = 0.0;
}

void LinearA2DConverter::setNAN(int ichan)
{
    _b[ichan] = floatNAN;
    _mx[ichan] = floatNAN;
}

void LinearA2DConverter::setNAN()
{
    for (int ichan = 0;  ichan < _maxNumChannels; ichan++)
        setNAN(ichan);
}


PolyA2DConverter::PolyA2DConverter(int nchan, int ncoefs):
    A2DConverter(nchan, ncoefs),
    _d(new float*[nchan])
{
    for (int i = 0; i < _maxNumChannels; i++) {
        _d[i] = new float[_ncoefs];
        for (int j = 0; j < _ncoefs; j++)
            _d[i][j] = (j == 1 ? 1.0 : 0.0);
    }
}

PolyA2DConverter::~PolyA2DConverter()
{
    for (int i = 0; i < _maxNumChannels; i++)
        delete [] _d[i];
    delete [] _d;
}

#ifdef DEFINE_A2DC_COPY_ASSIGN
PolyA2DConverter::PolyA2DConverter(const PolyA2DConverter& x):
    PolyA2DConverter(x._maxNumChannels, x._ncoefs)
{
    for (int i = 0; i < _maxNumChannels; i++) {
        _d[i] = new float[_ncoefs];
        for (int j = 0; j < _ncoefs; j++)
            _d[i][j] = x._d[i][j];
    }
}

PolyA2DConverter& PolyA2DConverter::operator=(const PolyA2DConverter& x)
{
    if (&x == this) return *this;
    if (_maxNumChannels != x._maxNumChannels || _ncoefs !=  x._ncoefs) {
        for (int i = 0; i < _maxNumChannels; i++) delete [] _d[i];
        delete [] _d;
        _d = new float*[x._maxNumChannels];
        for (int i = 0; i < x._maxNumChannels; i++)
            _d[i] = new float[x._ncoefs];
        _maxNumChannels = x._maxNumChannels;
        _ncoefs = x._ncoefs;
    }

    for (int i = 0; i < _maxNumChannels; i++)
        for (int j = 0; j < _ncoefs; j++)
            _d[i][j] = x._d[i][j];
    return *this;
}
#endif

void PolyA2DConverter::set(int ichan, const float* d, int nd)
{
    assert(ichan >= 0 && ichan < _maxNumChannels);
    assert(nd <= _ncoefs);
    int i;
    for (i = 0; i < nd; i++)
        _d[ichan][i] = d[i];
    for (; i < _ncoefs; i++)
        _d[ichan][i] = 0.0;
}

void PolyA2DConverter::get(int ichan, float* d, int nd) const
{
    assert(ichan >= 0 && ichan < _maxNumChannels);
    assert(nd >= _ncoefs);
    int i;
    for (i = 0; i < _ncoefs; i++) d[i] = _d[ichan][i];
    for (; i < nd; i++) d[i] = 0.0;
}

void PolyA2DConverter::setNAN(int ichan)
{
    for (int i = 0; i < _ncoefs; i++)
        _d[ichan][i] = floatNAN;
}

void PolyA2DConverter::setNAN()
{
    for (int ichan = 0;  ichan < _maxNumChannels; ichan++)
        setNAN(ichan);
}

float PolyA2DConverter::convert(int ichan, float counts) const
{
    double out = counts;
    if (_ncoefs > 0)
    {
        int corder = _ncoefs - 1;
        out = _d[ichan][corder];
        for (int k = 1; k < _ncoefs; k++)
          out = _d[ichan][corder-k] + counts * out;

    }
    return out;
}

void A2DConverter::readCalFile(CalFile* calFile, dsm_time_t tt)
{
    // Can throw:
    // EOFException, IOException, ParseException.

    // Read CalFile  containing the following fields after the time
    // gain bipolar(1=true,0=false) intcp0 slope0 intcp1 slope1 ... intcp7 slope7
    while(tt >= calFile->nextTime().toUsecs()) {
        int nd = 2 + getNumConfigChannels() * _ncoefs;
        float d[nd];

        n_u::UTime calTime;
        int n = calFile->readCF(calTime, d,nd);
        if (n < 2) continue;

        int nchanInRecord = (n - 2) / _ncoefs;
        int cgain = (int)d[0];
        int cbipolar = (int)d[1];
        int ichan;
        for (ichan = 0; ichan < getNumConfigChannels(); ichan++) {
            if (_gain[ichan] > 0 &&
                (cgain < 0 || _gain[ichan] == cgain) &&
                (cbipolar < 0 || _bipolar[ichan] == cbipolar)) {
                if (ichan < nchanInRecord) {
                    set(ichan, d + 2 + _ncoefs * ichan, _ncoefs);
                }
                else {
                    // matching gain and bipolar but short record
                    n_u::Logger::getInstance()->log(LOG_WARNING,
                        "%s: record %d has %d values, should have at least %d (2 + nchan=%d X ncoef=%d)",
                        calFile->getCurrentFileName().c_str(),
                        calFile->getLineNumber()-1,
                        n, nd, getNumConfigChannels(), _ncoefs);
                    setNAN(ichan);
                    break;
                }
            }
        }
    }
}
