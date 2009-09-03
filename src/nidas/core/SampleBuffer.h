/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-05-13 11:53:37 -0600 (Wed, 13 May 2009) $

    $LastChangedRevision: 4598 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/SampleSorter.h $
 ********************************************************************

*/

#ifndef NIDAS_CORE_SAMPLEBUFFER_H
#define NIDAS_CORE_SAMPLEBUFFER_H

#include <iostream>

#include <nidas/core/SampleThread.h>
#include <nidas/core/SampleSourceSupport.h>

namespace nidas { namespace core {

/**
 * A SampleClient that buffers its received samples,
 * using an STL list, and then sends the
 * buffered samples onto its SampleClients.
 * It is like a SampleSorter, with a sorter length of 0.
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
     * Calls finish() all all SampleClients.
     * Implementation of SampleSource::flush().
     */
    void flush() throw()
    {
        _source.flush();
    }

    void interrupt();

    bool receive(const Sample *s) throw();

    size_t size() const { return _samples.size(); }

    void setLengthSecs(float val)
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
     * flush all samples from buffer, distributing them to SampleClients.
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

    /**
     * Thread run function.
     */
    int run() throw(nidas::util::Exception);

    std::list<const Sample*> _samples;

    SampleSourceSupport _source;

    /**
     * Utility function to decrement the heap size after writing
     * one or more samples. If the heapSize has has shrunk below
     * heapMax then signal any threads waiting on heapCond.
     */
    void inline heapDecrement(size_t bytes);

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

    /**
     * Is this sorter running in real-time?  If so then we can
     * screen for bad time-tags by checking against the
     * system clock, which is trusted.
     */
    bool _realTime;

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
