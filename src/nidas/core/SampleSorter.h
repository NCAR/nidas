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

#include <nidas/core/SampleSource.h>
#include <nidas/core/SampleClient.h>
#include <nidas/core/SortedSampleSet.h>
#include <nidas/core/DSMTime.h>

#ifdef USE_LOOPER
#include <nidas/core/LooperClient.h>
#endif

#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>

namespace nidas { namespace core {

/**
 * A SampleClient that sorts its received samples,
 * using an STL multiset, and then sends the
 * sorted samples onto its SampleClients.
 * One specifies a sorter length in the constructor.
 * Samples whose time-tags are less than the time-tag
 * of the latest sample received, minus the sorter length,
 * are sent on to the SampleClients.
 * This is implemented as a Thread, which must be started,
 * otherwise the sorter will grow and no samples will be
 * sent to clients.
 * This can be a client of multiple DSMSensors, so that the
 * processed samples are sorted in time.
 */
class SampleSorter : public nidas::util::Thread,
	public SampleClient, public SampleSource

#ifdef USE_LOOPER
	, public LooperClient
#endif
{
public:

    /**
     * Constructor.
     * @param sorterLength Length of the SampleSorter, in milliseconds.
     */
    SampleSorter(const std::string& name);

    virtual ~SampleSorter();

    void interrupt();

    bool receive(const Sample *s) throw();

    size_t size() const { return _samples.size(); }

    void setLengthMsecs(int val)
    {
        _sorterLengthUsec = val * USECS_PER_MSEC;
    }

    int getLengthMsecs() const
    {
        return _sorterLengthUsec / USECS_PER_MSEC;
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

    const std::list<const SampleTag*>& getSampleTags() const
    {
        return _sampleTags;
    }

    void addSampleTag(const SampleTag* tag)
    	throw(nidas::util::InvalidParameterException)
    {
        if (find(_sampleTags.begin(),_sampleTags.end(),tag) == _sampleTags.end())
            _sampleTags.push_back(tag);
    }

    /**
     * Add a Client for a given SampleTag.
     */
    void addSampleTag(const SampleTag* tag,SampleClient*)
    	throw(nidas::util::InvalidParameterException);

    /**
     * Total number of input bytes.
     */
    long long getNumInputBytes() const
    {
        return _nInputBytes;
    }

    /**
     * Number of input samples.
     */
    size_t getNumInputSamples() const { return _nInputSamples; }

    /**
     * Timetag of most recent sample inserted in the sorter.
     */
    dsm_time_t getLastInputTimeTag() const { return _lastInputTimeTag; }

    /**
     * Timetag of most recent sample output from the sorter.
     */
    dsm_time_t getLastOutputTimeTag() const { return _lastOutputTimeTag; }

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


protected:

    /**
     * Thread run function.
     */
    virtual int run() throw(nidas::util::Exception);

    /**
     * Length of SampleSorter, in micro-seconds.
     */
    int _sorterLengthUsec;

    SortedSampleSet _samples;

private:

    /**
     * Utility function to decrement the heap size after writing
     * one or more samples. If the heapSize has has shrunk below
     * heapMax then signal any threads waiting on heapCond.
     */
    void inline heapDecrement(size_t bytes);

    dsm_time_t _lastInputTimeTag;

    dsm_time_t _lastOutputTimeTag;

    long long _nInputBytes;

    size_t _nInputSamples;

    size_t _nOutputSamples;

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

    bool _doFlush;

    bool _flushed;

    std::list<const SampleTag*> _sampleTags;

    std::map<dsm_sample_id_t,SampleClientList> _clientsBySampleId;

    nidas::util::Mutex _clientMapLock;

    SampleT<char> _dummy;

    /**
     * Is this sorter running in real-time?  If so then we can
     * screen for bad time-tags by checking against the
     * system clock, which is trusted.
     */
    bool _realTime;

    /**
     * No assignment.
     */
    SampleSorter& operator=(const SampleSorter&);

};

}}	// namespace nidas namespace core

#endif
