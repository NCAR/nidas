// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_SAMPLESTATS_H
#define NIDAS_CORE_SAMPLESTATS_H

#include <nidas/core/Sample.h>

namespace nidas { namespace core {

/**
 * A source of samples. A SampleSource maintains a list
 * of SampleClients.  When a SampleSource has a Sample ready,
 * it will call the receive method of all its SampleClients.
 * SampleClients register/unregister with a SampleSource via
 * the addSampleClient/removeSampleClient methods.
 */
class SampleStats {
public:

    SampleStats() : _nbytes(0),_lastTT(0),_nsamples(0) {}

    long long getNumBytes() const
    {
        return _nbytes;
    }

    void addNumBytes(int n)
    {
        _nbytes += n;
    }

    size_t getNumSamples() const
    {
        return _nsamples;
    }
    void addNumSamples(int n)
    {
        _nsamples += n;
    }

    dsm_time_t getLastTimeTag() const
    {
        return _lastTT;
    }

    void setLastTimeTag(dsm_time_t val)
    {
        _lastTT = val;
    }

private:
    long long _nbytes;
    dsm_time_t _lastTT;
    size_t _nsamples;

};

}}	// namespace nidas namespace core


#endif
