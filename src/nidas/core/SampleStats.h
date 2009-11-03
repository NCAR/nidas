
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-05-13 11:20:32 -0600 (Wed, 13 May 2009) $

    $LastChangedRevision: 4597 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/SampleSource.h $


*/

#ifndef NIDAS_CORE_SAMPLESTATS_H
#define NIDAS_CORE_SAMPLESTATS_H

#include <nidas/core/DSMTime.h>

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
