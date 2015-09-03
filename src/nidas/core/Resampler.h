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
     * Both SampleClient and SampleSource have a flush() method.
     * Redeclaring it here as pure virtual removes the ambiguity.
     */
    virtual void flush() throw() = 0;

    /**
     * Connect the resampler to a source.
     */
    virtual void connect(SampleSource* source) throw(nidas::util::InvalidParameterException) = 0;

    virtual void disconnect(SampleSource* source) throw() = 0;

};

}}	// namespace nidas namespace core

#endif
