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

#ifndef NIDAS_DYNLD_SAMPLEOUTPUTSTREAM_H
#define NIDAS_DYNLD_SAMPLEOUTPUTSTREAM_H


#include <nidas/core/SampleOutput.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * A class for serializing Samples on an OutputStream.
 */
class SampleOutputStream: public SampleOutputBase
{
public:

    SampleOutputStream();

    /**
     * Create a SampleOutputStream with a connected IOChannel.
     */
    SampleOutputStream(IOChannel* iochan,SampleConnectionRequester* rqstr=0);

    virtual ~SampleOutputStream();

    /**
     * Implementation of IOChannelRequester::connected().
     * How an IOChannel indicates that it has received a connection.
     */
    SampleOutput* connected(IOChannel* ochan) throw();

    /**
     * Get the IOStream of this SampleOutputStream.
     * SampleOutputStream owns the pointer and
     * will delete the IOStream in its destructor.
     * The IOStream is available after a SammpleOutputStream is 
     * constructed with an connected IOChannel, or after the connected()
     * method has been called and before close().
     */
    IOStream* getIOStream() { return _iostream; }

    void close() throw(nidas::util::IOException);

    bool receive(const Sample *s) throw();

    void flush() throw();

    size_t write(const void* buf, size_t len, bool streamFlush)
    	throw(nidas::util::IOException);

    /**
     * Outgoing data is buffered in an IOStream.
     * The stream will be flushed when the difference between
     * successive time tags exceeds this value.
     * This is a useful parameter for real-time applications.
     * @param val Number of seconds between physical writes.
     *        Default is set in SampleOutputBase.
     */
    void setLatency(float val)
    	throw(nidas::util::InvalidParameterException);

protected:

    SampleOutputStream* clone(IOChannel* iochannel);

    /**
     * Copy constructor, with a new IOChannel.
     */
    SampleOutputStream(SampleOutputStream&,IOChannel*);

    size_t write(const Sample* samp, bool streamFlush) throw(nidas::util::IOException);

    IOStream* _iostream;

private:

    /**
     * Maximum number of microseconds between physical writes.
     */
    int _maxUsecs;

    /**
     * Timetag of last flush of IOStream.
     */
    dsm_time_t _lastFlushTT;

    /**
     * No copy.
     */
    SampleOutputStream(const SampleOutputStream&);

    /**
     * No assignment.
     */
    SampleOutputStream& operator=(const SampleOutputStream&);

};

}}	// namespace nidas namespace core

#endif
