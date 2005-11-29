/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <vector>

#include <SampleSorter.h>
#include <DSMTime.h>

#include <atdUtil/Logger.h>

// #include <unistd.h>	// for sleep
#include <iostream>

using namespace dsm;
using namespace std;

SampleSorter::SampleSorter(int sorterLength, const string& name) :
    Thread(name),sorterLengthUsec(sorterLength*USECS_PER_MSEC),
    sampleSetCond("sampleSetCond"),
    heapMax(1000000),heapSize(0),heapBlock(false),discardedSamples(0),
    discardWarningCount(1000)
{
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
}

SampleSorter::~SampleSorter()
{
    // make sure thread is not running.
    if (isRunning()) interrupt();
    if (!isJoined()) join();

    SortedSampleSet::iterator iv;
    for (iv = samples.begin(); iv != samples.end(); iv++) {
	const Sample *s = *iv;
	s->freeReference();
    }
    samples.clear();
}

/**
 * Thread function.
 */
int SampleSorter::run() throw(atdUtil::Exception) {

    cerr << "SampleSorter, sorterLengthUsec=" << sorterLengthUsec << endl;
    sampleSetCond.lock();
    for (;;) {

	if (amInterrupted()) break;

	SortedSampleSet::const_reverse_iterator latest = samples.rbegin();

	if (latest == samples.rend()) {	// empty set
	    sampleSetCond.wait();
	    continue;
	}
	dsm_time_t tt = (*latest)->getTimeTag() - sorterLengthUsec;
	dummy.setTimeTag(tt);
	// cerr << "tt=" << tt << endl;
	/*
	cerr << getFullName() << " samples.size=" <<
		samples.size() <<
		" tt=" << dummy.getTimeTag() << std::endl;
        */

	SortedSampleSet::const_iterator rsb = samples.begin();

	// get iterator pointing at first sample not less than dummy
	SortedSampleSet::const_iterator rsi = samples.lower_bound(&dummy);

	if (rsi == rsb) { // no aged samples
	    // If no aged samples but we're at the heap limit,
	    // then we need to extend the limit.
	    heapCond.lock();
	    if (heapSize > heapMax) {
	        heapMax = heapSize;
		heapCond.signal();
	    	atdUtil::Logger::getInstance()->log(LOG_NOTICE,
		    "increased heapMax to %d, # of samples=%d\n",
		    heapMax,size());
	    }
	    heapCond.unlock();
	    sampleSetCond.wait();
	    continue;
	}

	// grab the samples before the iterator
	std::vector<const Sample*> agedsamples(rsb,rsi);

#ifdef DEBUG
	cerr << getFullName() << " agedsamples.size=" <<
		agedsamples.size() << endl;
#endif

	// remove samples from sorted multiset
	samples.erase(rsb,rsi);

	// free the lock
	sampleSetCond.unlock();

	// loop over the aged samples
	std::vector<const Sample *>::const_iterator si;
	size_t ssum = 0;
	for (si = agedsamples.begin(); si < agedsamples.end(); ++si) {
	    const Sample *s = *si;
	    distribute(s);
	    ssum += s->getDataByteLength() + s->getHeaderLength();
	    s->freeReference();
	}
	heapDecrement(ssum);

	sampleSetCond.lock();
    }
    sampleSetCond.unlock();
    return RUN_OK;
}

void SampleSorter::interrupt() {
    // cerr << "SampleSorter::interrupt" << endl;
    sampleSetCond.lock();

    // After setting this lock, we know that the
    // consumer thread is either:
    //	* waiting on sampleSetCond,
    // 	* distributing samples or,
    //	* hasn't started looping,
    // since those are the only times sampleSetCond is unlocked.

    // If we only did a signal without locking, it
    // the interrupt could be missed.

    Thread::interrupt();
    sampleSetCond.unlock();
    sampleSetCond.signal();
    // cerr << "SampleSorter::interrupt done" << endl;
}

// We've removed some samples from the heap. Decrement heapSize
// and signal waiting threads if the heapSize
// has shrunk to less than heapMax bytes.
void inline SampleSorter::heapDecrement(size_t bytes)
{
    heapCond.lock();
    if (!heapBlock) heapSize -= bytes;
    else {
	if (heapSize > heapMax) {	// SampleSource must be waiting
	    heapSize -= bytes;
	    if (heapSize <= heapMax) {
		// cerr << "signalling heap waiters, heapSize=" << heapSize << endl;
		heapCond.signal();
	    }
	}
	else heapSize -= bytes;
    }
    heapCond.unlock();
}

/**
 * flush all samples from buffer, distributing them to SampleClients.
 */
void SampleSorter::flush() throw (atdUtil::IOException)
{
    sampleSetCond.lock();
    SortedSampleSet tmpset = samples;
    samples.clear();
    sampleSetCond.unlock();

    SortedSampleSet::const_iterator si;
    size_t ssum = 0;
    for (si = tmpset.begin(); si != tmpset.end(); ++si) {
	const Sample *s = *si;
	distribute(s);
	ssum += s->getDataByteLength() + s->getHeaderLength();
	s->freeReference();
    }
    heapDecrement(ssum);
}

bool SampleSorter::receive(const Sample *s) throw()
{

    // Check if the heapSize will exceed heapMax
    size_t slen = s->getDataByteLength() + s->getHeaderLength();
    heapCond.lock();
    if (!heapBlock) {
        if (heapSize + slen > heapMax) {
	    heapCond.unlock();
	    if (!(discardedSamples++ % discardWarningCount))
	    	atdUtil::Logger::getInstance()->log(LOG_WARNING,
	"%d discarded samples because heapSize(%d) + sampleSize(%d) is > than heapMax(%d)\n",
		discardedSamples,heapSize,slen,heapMax);
	    return false;
	}
	heapSize += slen;
    }
    else {
	heapSize += slen;
	// if heapMax will be exceeded, then wait until heapSize comes down
	while (heapSize > heapMax) {
	    // Wait until we've distributed enough samples
	    // cerr << "waiting on heap condition, heapSize=" << heapSize << endl;
	    sampleSetCond.signal();
	    heapCond.wait();
	    // cerr << "received heap signal, heapSize=" << heapSize << endl;
	}
    }
    heapCond.unlock();

    sampleSetCond.lock();
    samples.insert(samples.end(),s);
    sampleSetCond.unlock();
    s->holdReference();

    sampleSetCond.signal();

    return true;
}

