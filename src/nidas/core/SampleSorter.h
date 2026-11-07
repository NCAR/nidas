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

#ifndef NIDAS_CORE_SAMPLESORTER_H
#define NIDAS_CORE_SAMPLESORTER_H

#include "SampleThread.h"
#include "SampleSourceSupport.h"
#include "SortedSampleSet.h"

#include <deque>

namespace nidas { namespace core {

/**
 * SampleSorter is a SampleClient and SampleSource that buffers its received
 * samples.  By default it sorts the samples by time tag with a
 * SortedSampleSet, then distributes the sorted samples to its SampleClients.
 * The time period of the sorting is specified with setLengthSecs(). Samples
 * whose time-tags are previous to the time-tag of the latest sample received
 * minus the sorter length, are sent on to the SampleClients. This is
 * implemented as a Thread, which must be started, otherwise the sorter will
 * grow and no samples will be sent to clients. This can be a client of
 * multiple SampleSources, so the samples from all the sources are sorted in
 * time before being distributed.  If the sorting length is zero, then the
 * samples are not sorted, but they are still buffered.  The SampleSorter can
 * accumulate Samples in its buffer from another thread without blocking the
 * SampleSource to which it is connected, while the sample distribution
 * happens in this sorter thread.
 *
 * Client synchronization can be forced by setting real-time to false and
 * sorter length to zero.  That disables buffering and the separate thread.
 * All samples recieved by the sorter are immediately distributed to the
 * clients.
 */
class SampleSorter : public SampleThread
{
public:

    /**
     * Construct a default SampleSorter, with sorter length 250 milliseconds,
     * heap blocking disabled, and a maximum heap size of 50 MB.  See
     * setHeapMax() for details on heap behavior.  See setSorterLengthSecs()
     * for details on sorter length and how to disable sorting.
     * 
     * @ param raw: boolean indicating whether this SampleSorter
     *   is for raw or processed samples. Clients can query this
     *   value, and it controls what is returned by
     *   getRawSampleSource() and getProcessedSampleSource().
     */
    SampleSorter(const std::string& name, bool raw);

    virtual ~SampleSorter();

    /**
     * Return true if this SampleSorter has a sorter length greater than zero
     * and is therefore sorting samples. Otherwise return false.
     */
    bool isSorting() const;

    /**
     * Return true if this SampleSorter is synchronized with its clients,
     * meaning samples are distributed to clients in the calling thread as
     * they are received, In other words, the receive() method returns only
     * after all clients have returned from their receive() calls.
     */
    bool isSynchronized() const;

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
     * How to tell this SampleSorter what sample tags it will be
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
     * Implementation of SampleSource::flush().
     * flush all samples from buffer, distributing them to SampleClients.
     * The first caller will block until the buffer is empty.
     */
    void flush() throw();

    /**
     * Interrupt sorting thread.
     */
    void interrupt();

    /**
     * Implementation of SampleClient::receive().
     */
    bool receive(const Sample *s) throw();

    /**
     * Current number of samples in the sorter.
     * This method does not hold a lock to force exclusive
     * access to the sample container. Therefore this is only an
     * instantaneous check and shouldn't be used by methods
     * in this class when exclusive access is required.
     */
    size_t size() const;

    /**
     * Set the length of the sorter in seconds.  If the length is zero, then
     * sorting is disabled and the SampleSorter behaves like a FIFO buffer. If
     * the thread is already running, then the call is ignored.
     */
    void setLengthSecs(float val);

    float getLengthSecs() const;

    /**
     * Set the maximum amount of memory to use for all the Samples in the
     * buffer. If heap blocking is disabled, then samples which would exceed
     * the maximum heap size are discarded.  If heap blocking is enabled, then
     * the receive() call will block until the heap size comes down to some
     * threshold below the maximum.  The maximum heap is a fixed size except
     * when the heap is not large enough to contain all the samples that fit
     * within the sorter length, in which case it is increased by 50%
     * automatically, as many times as needed. See setHeapBlock().
     * 
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

    /**
     * Number of samples discarded because they would have exceeded the
     * maximum heap size (_heapSize > _heapMax) with heapBlock == true.
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
     * Number of early samples, which may not be sorted.
     */
    size_t getNumEarlySamples() const
    {
        return _earlySamples;
    }

    /**
     * Set whether this sorter is running in real-time.  If real-time, and
     * sorting, then screen for bad time-tags by checking against the system
     * clock.  If not sorting, then setting real-time to false forces
     * synchronization with the clients, meaning every Sample passed to
     * receive() will be immediately distributed to the clients.
     */
    void setRealTime(bool val) 
    {
        _realTime = val;
    }

    bool getRealTime() const
    {
        return _realTime;
    }

    /**
     * When aging-off samples, cache this number of samples with
     * the latest time tags. This number, or fewer, of samples
     * with anomalous, late, time tags will not effect the sorting.
     * If the late sample cache size is zero, a jump ahead in time
     * greater than the length of the sorter in seconds 
     * will cause the sorter to be emptied and the sorting
     * effectively disabled until samples within the sorter
     * length of the bad sample are encountered.
     */
    void setLateSampleCacheSize(unsigned int val)
    {
        _lateSampleCacheSize = val;
    }

    unsigned int getLateSampleCacheSize() const
    {
        return _lateSampleCacheSize;
    }

private:

    SampleSourceSupport _source;

    /**
     * Thread run function.
     */
    int run() throw(nidas::util::Exception);

    /**
     * Return the next set of sorted samples that are aged off. The samples
     * are returned in the provided vector. If @p flush is true, all samples
     * are returned regardless of age.
     */
    void getNextSortedSamples(std::vector<const Sample*>& agedsamples,
                              bool flush);

    /**
     * Return the next samples available, whether from the FIFO queue or the
     * sorted set. If @p flush is true, all samples are returned regardless of
     * age.  The sample set lock must already be held.
     */
    void getNextSamples(std::vector<const Sample*>& samples, bool flush);

    /**
     * Length of SampleSorter, in micro-seconds.
     */
    unsigned int _sorterLengthUsec;

    /// cache the sorting state once the thread starts
    bool _sorting;

    /**
     * When sorter length is non-zero, this structure holds the samples sorted
     * by time tag.  Otherwise the samples are kept in a FIFO queue.  Only one
     * structure is used at a time, so _sampleSetCond applies to both.
     */
    SortedSampleSet _samples;
    std::deque<const Sample*> _fifoSamples;

    /**
     * Utility function to decrement the heap size after writing
     * one or more samples. If the heapSize has has shrunk below
     * heapMax then signal any threads waiting on heapCond.
     */
    void heapDecrement(size_t bytes);

    /**
     * Return true if there is room for a sample of the given length within
     * the heap size limits.  If there is not room and heap blocking is
     * enabled, then wait until space becomes available.  Otherwise return
     * false, indicating the sample should be dropped.
     */
    bool heapAvailable(unsigned int slen);

    /**
     * Return true if the sample time tag looks valid relative to the system
     * clock, which of course is only a useful check in real-time.
     */
    bool validTime(const Sample *s);

    /**
     * Warn if a sample is earlier than the current sorter window.
     */
    void checkEarlySample(const Sample *s);

    nidas::util::Cond _sampleSetCond;

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
     * in the sample buffer exceeds _heapMax.
     * If _heapBlock is true and _heapSize exceeds _heapMax,
     * then any threads which are calling SampleSorter::receive()
     * to insert a sample to this sorter will block on _heapCond
     * until sample consumers have reduced _heapSize to less than
     * _heapMax.
     * If _heapBlock is false and _heapSize exceeds _heapMax
     * then samples are discarded until _heapSize is less than _heapMax.
     */
    bool _heapBlock;

    nidas::util::Cond _heapCond;

    bool _heapExceeded;

    /**
     * Number of samples discarded because of _heapSize > _heapMax
     * and heapBlock == true.
     */
    unsigned int _discardedSamples;

    /**
     * Number of samples discarded because getRealTime() is true
     * and the samples have timetags later than the system clock.
     */
    unsigned int _realTimeFutureSamples;

    /**
     * Samples which are earlier than the current latest sample
     * in the sorter minus the sorter length. These samples
     * have arrived too late to be necessarily sorted. They
     * may or may not be sorted, depending on how often the
     * sorting buffer is read.
     */
    unsigned int _earlySamples;

    /**
     * How often to log warnings about discardedSamples.
     */
    int _discardWarningCount;

    /**
     * How often to log warnings about early samples.
     */
    int _earlyWarningCount;

    bool _doFlush;

    bool _flushed;

    SampleT<char> _dummy;

    /**
     * Is this sorter running in real-time?  If so then we can
     * screen for bad time-tags by checking against the
     * system clock, which is trusted.
     */
    bool _realTime;

    long long _maxSorterLengthUsec;

    unsigned int _lateSampleCacheSize;

    /**
     * No copy.
     */
    SampleSorter(const SampleSorter&);

    /**
     * No assignment.
     */
    SampleSorter& operator=(const SampleSorter&);

};

}}	// namespace nidas namespace core

#endif
