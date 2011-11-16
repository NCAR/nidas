// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
     * Connect the resampler to a source.
     */
    virtual void connect(SampleSource* source) throw(nidas::util::InvalidParameterException) = 0;

    virtual void disconnect(SampleSource* source) throw() = 0;

};

}}	// namespace nidas namespace core

#endif
