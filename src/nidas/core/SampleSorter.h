/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_SAMPLESORTER_H
#define NIDAS_CORE_SAMPLESORTER_H

#include <nidas/core/SampleThread.h>
#include <nidas/core/SampleSourceSupport.h>
#include <nidas/core/SortedSampleSet.h>

namespace nidas { namespace core {

/**
 * A SampleClient that sorts its received samples,
 * using an STL multiset, and then sends the
 * sorted samples onto its SampleClients.
 * The time period of the sorting is specified with
 * setLengthSecs().
 * Samples whose time-tags are previous to the time-tag
 * of the latest sample received minus the sorter length,
 * are sent on to the SampleClients.
 * This is implemented as a Thread, which must be started,
 * otherwise the sorter will grow and no samples will be
 * sent to clients.
 * This can be a client of multiple SampleSources, so that the
 * distributed samples are sorted in time.
 */
class SampleSorter : public SampleThread
{
public:

    /**
     * Constructor.
     * @ param raw: boolean indicating whether this SampleSorter
     *   is for raw or processed samples. Clients can query this
     *   value, and it controls what is returned by
     *   getRawSampleSource() and getProcessedSampleSource().
     */
    SampleSorter(const std::string& name,bool raw);

    virtual ~SampleSorter();

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
     * Calls finish() all all SampleClients.
     * Implementation of SampleSource::flush().
     */
    void flush() throw()
    {
        _source.flush();
    }

    void interrupt();

    /**
     * Implementation of SampleClient::receive().
     */
    bool receive(const Sample *s) throw();

    /**
     * Current number of samples in the sorter.
     */
    size_t size() const { return _samples.size(); }

    void setLengthSecs(float val)
    {
        _sorterLengthUsec = (unsigned int)((double)val * USECS_PER_SEC);
    }

    float getLengthSecs() const
    {
        return (double)_sorterLengthUsec / USECS_PER_SEC;
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

    /**
     * flush all samples from buffer, distributing them to SampleClients.
     * Implementation of SampleClient::finish().
     */
    void finish() throw();

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

private:

    SampleSourceSupport _source;

    /**
     * Thread run function.
     */
    int run() throw(nidas::util::Exception);

    /**
     * Length of SampleSorter, in micro-seconds.
     */
    unsigned int _sorterLengthUsec;

    SortedSampleSet _samples;

    /**
     * Utility function to decrement the heap size after writing
     * one or more samples. If the heapSize has has shrunk below
     * heapMax then signal any threads waiting on heapCond.
     */
    void heapDecrement(size_t bytes);

    nidas::util::Cond _sampleSetCond;

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

    bool _heapExceeded;

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

    bool _doFinish;

    bool _finished;

    SampleT<char> _dummy;

    /**
     * Is this sorter running in real-time?  If so then we can
     * screen for bad time-tags by checking against the
     * system clock, which is trusted.
     */
    bool _realTime;

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
