/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_RESAMPLER_H
#define NIDAS_CORE_RESAMPLER_H

#include <nidas/core/SampleSource.h>
#include <nidas/core/SampleClient.h>
#include <nidas/core/SampleInput.h>

namespace nidas { namespace core {

/**
 * Interface for a resampler, simply a SampleClient and a SampleSource.
 */
class Resampler : public SampleClient, public SampleSource {
public:

    virtual ~Resampler() {}

    /**
     * Connect the resampler to an input.
     */
    virtual void connect(SampleInput* input) throw(nidas::util::IOException) = 0;

    virtual void disconnect(SampleInput* input) throw(nidas::util::IOException) = 0;

};

}}	// namespace nidas namespace core

#endif
