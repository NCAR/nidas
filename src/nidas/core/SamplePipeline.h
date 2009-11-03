/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-06-25 11:42:06 -0600 (Thu, 25 Jun 2009) $

    $LastChangedRevision: 4698 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/SampleInput.h $
 ********************************************************************
*/

#ifndef NIDAS_CORE_SAMPLEPIPELINE_H
#define NIDAS_CORE_SAMPLEPIPELINE_H

#include <nidas/core/SampleInput.h>
#include <nidas/core/SampleThread.h>

namespace nidas { namespace core {

class DSMConfig;
class DSMSensor;

/**
 * SamplePipeline sorts samples that are coming from one
 * or more inputs.  Samples can then be passed onto sensors
 * for processing, and then sorted again.
 *
 * SamplePipeline makes use of two SampleSorters, one called rawSorter
 * and procSorter.
 *
 * rawSorter is a client of one or more inputs (the text arrows
 * show the sample flow):
 *
 * input ----v
 * input --> _rawSorter
 * input ----^
 *
 * After sorting the samples, rawSorter passes them onto the two
 * types of SampleClients that have registered with SamplePipeline.
 * SampleClients that have registered with
 * SamplePipeline::addSampleClient will receive their raw samples
 * directly from rawSorter.
 *
 * rawSorter -> sampleClients
 *
 * SampleClients that have registered with
 * SamplePipeline::addProcessedSampleClient will receive their
 * samples indirectly:
 * 
 * rawSorter -> sensor -> procSorter -> processedSampleClients
 *
 * rawSorter provides sorting of the samples from the various inputs.
 *
 * procSorter provides sorting of the processed samples.
 * DSMSensors are apt to create processed samples with different
 * time-tags than the input raw samples, therefore they need
 * to be sorted again.
 */
class SamplePipeline: public SampleSource
{
public:
    SamplePipeline();

    virtual ~SamplePipeline();

    std::string getName() const { return _name; }

    SampleSource* getRawSampleSource()
    {
        rawinit();
        return _rawSorter;
    }

    SampleSource* getProcessedSampleSource()
    {
        procinit();
        return this;
    }

    /**
     * Add an input to be merged and sorted.
     * SamplePipeline does not own the SampleSource.
     */
    void connect(SampleSource* src) throw();

    /**
     * Remove a SampleSource from the SamplePipeline.
     */
    void disconnect(SampleSource* src) throw();

    void addSampleTag(const SampleTag* tag) throw()
    {
        return _procSorter->addSampleTag(tag);
    }

    void removeSampleTag(const SampleTag* tag) throw()
    {
        return _procSorter->removeSampleTag(tag);
    }

    std::list<const SampleTag*> getSampleTags() const
    {
        return _procSorter->getSampleTags();
    }

    SampleTagIterator getSampleTagIterator() const
    {
        return _procSorter->getSampleTagIterator();
    }

    /**
     * Add a SampleClient that wants all processed samples.
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClient(SampleClient* client) throw();

    void removeSampleClient(SampleClient* client) throw();

    /**
     * Add a SampleClient that wants samples which have been
     * merged from various inputs, sorted, processed through a
     * certain DSMSensor, and then re-sorted again.
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClientForTag(SampleClient*,const SampleTag*) throw();

    void removeSampleClientForTag(SampleClient*,const SampleTag*) throw();

    /**
     * How many SampleClients are currently in my list.
     */
    int getClientCount() const throw()
    {
        return _procSorter->getClientCount();
    }

#ifdef NEEDED
    /**
     * Distribute a sample to the clients of processed samples.
     */
    void distribute(const Sample* s) throw()
    {
        return _procSorter->distribute(s);
    }
#endif

    void flush() throw()
    {
        if (_rawSorter) _rawSorter->finish();
        if (_procSorter) _procSorter->finish();
    }

    // void finish() throw();

    void setRealTime(bool val) 
    {
        _realTime = val;
        // If running in real-time, don't block threads when heap is maxed.
        setHeapBlock(!val);
    }

    bool getRealTime() const
    {
        return _realTime;
    }

    const SampleStats& getSampleStats() const
    {
        return _procSorter->getSampleStats();
    }

    /**
     * Number of raw samples currently in the sorter.
     */
    size_t getSorterNumRawSamples() const
    {
        return _rawSorter->size();
    }

    /**
     * Number of processed samples currently in the sorter.
     */
    size_t getSorterNumProcSamples() const
    {
        return _procSorter->size();
    }

    /**
     * Current size in bytes of the raw sample sorter.
     */
    size_t getSorterNumRawBytes() const
    {
        return _rawSorter->getHeapSize();
    }

    /**
     * Current size in bytes of the processed sample sorter.
     */
    size_t getSorterNumProcBytes() const
    {
        return _procSorter->getHeapSize();
    }

    /**
     * Current size in bytes of the raw sample sorter.
     */
    size_t getSorterNumRawBytesMax() const
    {
        return _rawSorter->getHeapMax();
    }

    /**
     * Current size in bytes of the processed sample sorter.
     */
    size_t getSorterNumProcBytesMax() const
    {
        return _procSorter->getHeapMax();
    }

    /**
     * Number of raw samples discarded because sorter was getting
     * too big.
     */
    size_t getNumDiscardedRawSamples() const
    {
        return _rawSorter->getNumDiscardedSamples();
    }

    /**
     * Number of processed samples discarded because sorter was getting
     * too big.
     */
    size_t getNumDiscardedProcSamples() const
    {
        return _procSorter->getNumDiscardedSamples();
    }

    /**
     * Number of raw samples discarded because their timetags 
     * were in the future.
     */
    size_t getNumFutureRawSamples() const
    {
        return _rawSorter->getNumFutureSamples();
    }

    /**
     * Number of processed samples discarded because their timetags 
     * were in the future.
     */
    size_t getNumFutureProcSamples() const
    {
        return _procSorter->getNumFutureSamples();
    }

    /**
     * Set length of raw SampleSorter, in seconds.
     */
    void setRawSorterLength(float val)
    {
        _rawSorterLength = val;
    }

    float getRawSorterLength() const
    {
        return _rawSorterLength;
    }

    /**
     * Set length of processed SampleSorter, in seconds.
     */
    void setProcSorterLength(float val)
    {
        _procSorterLength = val;
    }

    float getProcSorterLength() const
    {
        return _procSorterLength;
    }

    /**
     * Set the maximum amount of heap memory to use for sorting samples.
     * @param val Maximum size of heap in bytes.
     * @see SampleSorter::setHeapMax().
     */
    void setRawHeapMax(size_t val) { _rawHeapMax = val; }

    size_t getRawHeapMax() const { return _rawHeapMax; }

    /**
     * Set the maximum amount of heap memory to use for sorting samples.
     * @param val Maximum size of heap in bytes.
     * @see SampleSorter::setHeapMax().
     */
    void setProcHeapMax(size_t val) { _procHeapMax = val; }

    size_t getProcHeapMax() const { return _procHeapMax; }

    /**
     * @param val If true, and heapSize exceeds heapMax,
     *   then wait for heapSize to be less then heapMax,
     *   which will block any SampleSources that are inserting
     *   samples into the raw buffer.  If false, then discard any
     *   samples that are received while heapSize exceeds heapMax.
     * @see SampleSorter::setHeapBlock().
     */
    void setHeapBlock(bool val) { _heapBlock = val; }

    bool getHeapBlock() const { return _heapBlock; }

    void setKeepStats(bool val)
    {
        _keepStats = val;
    }

private:

    void rawinit();

    void procinit();

    std::string _name;

    nidas::util::Mutex _rawMutex;

    SampleThread* _rawSorter;

    nidas::util::Mutex _procMutex;

    SampleThread* _procSorter;

    nidas::util::Mutex _sensorSetMutex;

    std::set<DSMSensor*> _sensorSet;

    std::list<const SampleTag*> _sampleTags;

    std::list<const DSMConfig*> _dsmConfigs;

    bool _realTime;

    /** seconds */
    float _rawSorterLength;

    /** seconds */
    float _procSorterLength;

    size_t _rawHeapMax;

    size_t _procHeapMax;

    bool _heapBlock;

    bool _keepStats;

    /**
     * No copying.
     */
    SamplePipeline(const SamplePipeline& x);

    /**
     * No assignment
     */
    SamplePipeline& operator = (const SamplePipeline& x);

};

}}	// namespace nidas namespace core

#endif
