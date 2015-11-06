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

#ifndef NIDAS_CORE_SAMPLETHREAD_H
#define NIDAS_CORE_SAMPLETHREAD_H

#include <nidas/core/SampleSource.h>
#include <nidas/core/SampleClient.h>

#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>

namespace nidas { namespace core {

/**
 * Interface for a Thread for buffering samples.
 * Samples are received by the SampleClient side of this
 * interface.  The SampleSource side runs in a separate
 * thread, sending out samples when they are ready.
 * This interface can be inplemented by a SampleSorter if
 * time-sorting is desired, or by a simple FIFO buffer.
 * Either implementation provides thread separation where
 * the thread using the SampleClient side could run as a 
 * high-priority real-time thread, doing time-critical time-tagging
 * and acquisition, and the SampleSource runs as a normal-priority
 * thread, doing less time-critical things like post-processing,
 * or sample archiving.
 */
class SampleThread : public nidas::util::Thread,
	public SampleClient, public SampleSource
{
public:

    SampleThread(const std::string& name): Thread(name) {}

    virtual ~SampleThread() {}

    /**
     * Both SampleClient and SampleSource have a flush() method.
     * Redeclaring it here as pure virtual removes the ambiguity.
     */
    virtual void flush() throw() = 0;

    virtual void setKeepStats(bool val) = 0;

    virtual bool getKeepStats() const = 0;

    /**
     * Number of samples that have not be distributed.
     */
    virtual size_t size() const = 0;

    virtual void setLengthSecs(float val) = 0;

    virtual float getLengthSecs() const = 0;

    /**
     * Set the maximum amount of heap memory to use for sorting samples.
     * @param val Maximum size of heap in bytes.
     */
    virtual void setHeapMax(size_t val) = 0;

    virtual size_t getHeapMax() const = 0;

    /**
     * Get the current amount of heap being used for sorting.
     */
    virtual size_t getHeapSize() const = 0;

    /**
     * @param val If true, and heapSize exceeds heapMax,
     *   then wait for heapSize to be less then heapMax,
     *   which will block any SampleSources that are inserting
     *   samples into this sorter.  If false, then discard any
     *   samples that are received while heapSize exceeds heapMax.
     */
    virtual void setHeapBlock(bool val) = 0;

    virtual bool getHeapBlock() const = 0;

    /**
     * Number of samples discarded because of _heapSize > _heapMax
     * and heapBlock == true.
     */
    virtual size_t getNumDiscardedSamples() const = 0;

    /**
     * Number of samples discarded because their timetags 
     * were in the future.
     */
    virtual size_t getNumFutureSamples() const = 0;

    /**
     * Is this thread running in real-time, meaning is it
     * handling recently sampled data?  If so then we can
     * screen for bad sample time-tags by checking against the
     * system clock, which is trusted.
     * Note that real-time here doesn't mean running at real-time priority.
     */
    virtual void setRealTime(bool val) = 0;

    virtual bool getRealTime() const = 0;

    virtual void setLateSampleCacheSize(unsigned int val) = 0;

    virtual unsigned int getLateSampleCacheSize() const = 0;

};

}}	// namespace nidas namespace core

#endif
