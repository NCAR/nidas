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

#ifndef NIDAS_CORE_SAMPLEINPUT_H
#define NIDAS_CORE_SAMPLEINPUT_H


#include <nidas/core/SampleSource.h>
#include <nidas/core/IOStream.h>
#include <nidas/core/SampleSorter.h>
#include <nidas/core/SampleInputHeader.h>

namespace nidas { namespace core {

class DSMConfig;
class DSMSensor;

/**
 * Interface of an input SampleSource. Typically a SampleInput is
 * reading serialized samples from a socket or file, and
 * then sending them on.
 *
 */
class SampleInput: public SampleSource, public IOChannelRequester, public DOMable
{
public:

    virtual ~SampleInput() {}

    virtual std::string getName() const = 0;

    virtual void setKeepStats(bool val) = 0;

    virtual const DSMConfig* getDSMConfig() const = 0;

    // virtual void init() throw(nidas::util::IOException) = 0;

    virtual void requestConnection(DSMService*) throw(nidas::util::IOException) = 0;

    virtual SampleInput* getOriginal() const = 0;

    /**
     * Read a buffer of data, serialize the data into samples,
     * and distribute() samples to the receive() method of my SampleClients.
     * This will perform only one physical read of the underlying device
     * and so is appropriate to use when a select() has determined
     * that there is data availabe on our file descriptor.
     * @return false: no data available for physical read, likely the result of
     *  doing a non-blocking read on an empty file descriptor.
     *  true: physical read did not necessarily consume all available data.
     */
    virtual bool readSamples() throw(nidas::util::IOException) = 0;

    /**
     * Blocking read of the next sample from the buffer. The caller must
     * call freeReference on the sample when they're done with it.
     */
    virtual Sample* readSample() throw(nidas::util::IOException) = 0;

    virtual void close() throw(nidas::util::IOException) = 0;

    virtual void setNonBlocking(bool val) throw(nidas::util::IOException) = 0;

    virtual bool isNonBlocking() const throw(nidas::util::IOException) = 0;

    virtual int getFd() const = 0;

};

}}	// namespace nidas namespace core

#endif
