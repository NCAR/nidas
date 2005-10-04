/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SAMPLESORTER_H
#define DSM_SAMPLESORTER_H

#include <SampleSource.h>
#include <SampleClient.h>
#include <SortedSampleSet.h>

#include <atdUtil/Thread.h>
#include <atdUtil/ThreadSupport.h>

namespace dsm {

/**
 * A SampleClient that sorts its received samples,
 * using an STL multimap, and then sends the
 * sorted samples onto its SampleClients.
 * One specifies a sorter length in the constructor.
 * Samples whose time-tags are less than the time-tag
 * of the latest sample received, minus the sorter length,
 * are sent on to the SampleClients.
 * This is implemented as a Thread, which must be started,
 * otherwise the sorter will grow and no samples will be
 * sent to clients.
 */
class SampleSorter : public atdUtil::Thread,
	public SampleClient, public SampleSource {
public:

    /**
     * Constructor.
     * @param sorterLength Length of the SampleSorter, in milliseconds.
     */
    SampleSorter(int sorterLength,const std::string& name);
    virtual ~SampleSorter();

    void interrupt();

    bool receive(const Sample *s) throw();

    size_t size() const { return samples.size(); }

    /**
     * Set the maximum amount of heap memory to use for sorting samples.
     * @param val Maximum size of heap in bytes.
     */
    void setHeapMax(size_t val) { heapMax = val; }

    size_t getHeapMax() const { return heapMax; }

    /**
     * Get the current amount of heap being used for sorting.
     */
    size_t getHeapSize() const { return heapSize; }

    /**
     * @param val If true, and heapSize exceeds heapMax,
     *   then wait for heapSize to be less then heapMax,
     *   which will block any SampleSources that are inserting
     *   samples into this sorter.  If false, then discard any
     *   samples that are received while heapSize exceeds heapMax.
     */
    void setHeapBlock(bool val) { heapBlock = val; }

    bool getHeapBlock() const { return heapBlock; }

    // void setDebug(bool val) { debug = val; }

    /**
     * flush all samples from buffer, distributing them to SampleClients.
     */
    void flush() throw (atdUtil::IOException);

protected:

    /**
     * Thread run function.
     */
    virtual int run() throw(atdUtil::Exception);

    /**
     * Length of SampleSorter, in milliseconds.
     */
    int sorterLengthUsec;

    SortedSampleSet samples;

    SampleT<char> dummy;

private:

    /**
     * Utility function to decrement the heap size after writing
     * one or more samples. If the heapSize has has shrunk below
     * heapMax then signal any threads waiting on heapCond.
     */
    void inline heapDecrement(size_t bytes);

    atdUtil::Cond sampleSetCond;

    // bool debug;

    /**
     * Limit on the maximum size of memory to use while buffering
     * samples.
     */
    size_t heapMax;

    /**
     * Current heap size, in bytes.
     */
    size_t heapSize;

    /**
     * If heapSize exceeds heapMax, do we wait for heapSize to
     * be less then heapMax, which will block any SampleSources
     * that are inserting samples into this sorter, or if
     * heapBlock is false, then discard any samples that
     * are received while heapSize exceeds heapMax.
     */
    bool heapBlock;

    atdUtil::Cond heapCond;

    size_t discardedSamples;

    int discardWarningCount;

};
}
#endif
