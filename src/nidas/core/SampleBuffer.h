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

#ifndef NIDAS_CORE_SAMPLEBUFFER_H
#define NIDAS_CORE_SAMPLEBUFFER_H

#include <iostream>
#include <deque>

#include "SampleThread.h"
#include "SampleSourceSupport.h"

namespace nidas { namespace core {

/**
 * A SampleClient that buffers its received samples,
 * using a pair of STL vectors, and then sends the
 * buffered samples onto its SampleClients.
 */
class SampleBuffer : public SampleThread
{
public:

    /**
     * Constructor.
     */
    SampleBuffer(const std::string& name,bool raw);

    virtual ~SampleBuffer();

    void setKeepStats(bool val)
    {
        _source.setKeepStats(val);
    }

    bool getKeepStats() const
    {
        return _source.getKeepStats();
    }

    SampleSource* getRawSampleSource()
    {
        return _source.getRawSampleSource();
    }

    SampleSource* getProcessedSampleSource()
    {
        return _source.getProcessedSampleSource();
    }

    /**
     * How to tell this SampleBuffer what sample tags it will be
     * sorting. SampleClients can then query it.
     */
    void addSampleTag(const SampleTag* tag) throw()
    {
        _source.addSampleTag(tag);
    }

    void removeSampleTag(const SampleTag* tag) throw()
    {
        _source.removeSampleTag(tag);
    }

    /**
     * Implementation of SampleSource::getSampleTags().
     */
    std::list<const SampleTag*> getSampleTags() const
    {
        return _source.getSampleTags();
    }

    /**
     * Implementation of SampleSource::getSampleTagIterator().
     */
    SampleTagIterator getSampleTagIterator() const
    {
        return _source.getSampleTagIterator();
    }

    /**
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClient(SampleClient* client) throw()
    {
        _source.addSampleClient(client);
    }

    void removeSampleClient(SampleClient* client) throw()
    {
        _source.removeSampleClient(client);
    }

    /**
     * Add a Client for a given SampleTag.
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClientForTag(SampleClient* client,const SampleTag* tag) throw()
    {
        _source.addSampleClientForTag(client,tag);
    }

    void removeSampleClientForTag(SampleClient* client,const SampleTag* tag) throw()
    {
        _source.removeSampleClientForTag(client,tag);
    }

    int getClientCount() const throw()
    {
        return _source.getClientCount();
    }

    const SampleStats& getSampleStats() const
    {
        return _source.getSampleStats();
    }

    /**
     * flush all samples from buffer, distributing them to SampleClients.
     */
    void flush() throw();

    void interrupt();

    /**
     * Insert a sample in the buffer, where it is then passed on to
     * SampleClients.  This method uses a lock to force thread exclusion so
     * that the SampleClient::receive() methods of downstream clients don't
     * have to worry about being reentrant.
     */
    bool receive(const Sample *s) throw();

    /**
     * Current number of samples in the buffer.
     * This method does hold a lock to force exclusive
     * access to the sample container. However, if double buffering
     * is used, a lock is not held on the buffer of samples currently
     * being sent on to clients.  Therefore this is only an
     * instantaneous check and should't be used by methods
     * in this class when exclusive access is required.
     */
    size_t size() const;

    void setLengthSecs(float)
    {
    }

    float getLengthSecs() const
    {
        return 0.0;
    }

    /**
     * Set the maximum amount of heap memory to use for sorting samples.
     * @param val Maximum size of heap in bytes.
     */
    void setHeapMax(size_t val) { _heapMax = val; }

    size_t getHeapMax() const { return _heapMax; }

    /**
     * Get the current amount of heap being used for sorting.
     */
    size_t getHeapSize() const { return _heapSize; }

    /**
     * @param val If true, and heapSize exceeds heapMax,
     *   then wait for heapSize to be less then heapMax,
     *   which will block any SampleSources that are inserting
     *   samples into this sorter.  If false, then discard any
     *   samples that are received while heapSize exceeds heapMax.
     */
    void setHeapBlock(bool val) { _heapBlock = val; }

    bool getHeapBlock() const { return _heapBlock; }

    // void setDebug(bool val) { debug = val; }

    /**
     * Number of samples discarded because of _heapSize > _heapMax
     * and heapBlock == true.
     */
    size_t getNumDiscardedSamples() const
    {
        return _discardedSamples;
    }

    /**
     * Number of samples discarded because their timetags 
     * were in the future.
     */
    size_t getNumFutureSamples() const
    {
        return _realTimeFutureSamples;
    }

    /**
     * Is this sorter running in real-time?  If so then we can
     * screen for bad time-tags by checking against the
     * system clock, which is trusted.
     */
    void setRealTime(bool val) 
    {
        _realTime = val;
    }

    bool getRealTime() const
    {
        return _realTime;
    }

    void setLateSampleCacheSize(unsigned int)
    {
    }

    unsigned int getLateSampleCacheSize() const
    {
        return 0;
    }

private:

    /**
     * Thread run function.
     */
    int run();

    std::vector<const Sample*> _sampleBufs[2];

    std::vector<const Sample*>* _inserterBuf;

    std::vector<const Sample*>* _consumerBuf;

    SampleSourceSupport _source;

    /**
     * Utility function to decrement the heap size after writing
     * one or more samples. If the heapSize has has shrunk below
     * heapMax then signal any threads waiting on heapCond.
     */
    void heapDecrement(size_t bytes);

    mutable nidas::util::Cond _sampleBufCond;

    nidas::util::Cond _flushCond;

    /**
     * Limit on the maximum size of memory to use while buffering
     * samples.
     */
    size_t _heapMax;

    /**
     * Current heap size, in bytes.
     */
    size_t _heapSize;

    /**
     * _heapBlock controls what happens when the number of bytes
     * in _samples exceeds _heapMax.
     * If _heapBlock is true and _heapSize exceeds _heapMax,
     * then any threads which are calling SampleSorter::receive
     * to insert a sample to this sorter will block on _heapCond
     * until sample consumers have reduced _heapSize to less than
     * _heapMax.
     * If _heapBlock is false and _heapSize exceeds _heapMax
     * then samples are discarded until _heapSize is less than _heapMax.
     */
    bool _heapBlock;

    nidas::util::Cond _heapCond;

    /**
     * Number of samples discarded because of _heapSize > _heapMax
     * and heapBlock == true.
     */
    size_t _discardedSamples;

    /**
     * Number of samples discarded because getRealTime() is true
     * and the samples have timetags later than the system clock.
     */
    size_t _realTimeFutureSamples;

    /**
     * How often to log warnings about discardedSamples.
     */
    int _discardWarningCount;

    bool _doFlush;

    bool _flushed;

    /**
     * Is this sorter running in real-time?  If so then we can
     * screen for bad time-tags by checking against the
     * system clock, which is trusted.
     */
    bool _realTime;

    size_t sizeNoLock() const;

    bool emptyNoLock() const;


    /**
     * No copy.
     */
    SampleBuffer(const SampleBuffer&);

    /**
     * No assignment.
     */
    SampleBuffer& operator=(const SampleBuffer&);

};

}}	// namespace nidas namespace core

#endif
