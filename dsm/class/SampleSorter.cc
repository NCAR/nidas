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

// #include <unistd.h>	// for sleep
#include <iostream>

using namespace dsm;
using namespace std;

SampleSorter::SampleSorter(int buflenInMilliSec_a) :
    Thread("SampleSorter"),buflenInMillisec(buflenInMilliSec_a),
    samplesAvail("samplesAvail"),threadSignalFactor(10),
    sampleCtr(0)
{
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
}

SampleSorter::~SampleSorter() {
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

    cerr << "SampleSorter, buflenInMillisec=" << buflenInMillisec << endl;
    for (;;) {

	// If another thread is interrupting us, we don't want the 
	// interrupt() method to finish between the time of the
	// check for amInterrupted and the wait(), otherwise we will
	// wait without seeing the interrupt. Therefore we check
	// for the interrupt after the lock.
	samplesAvail.lock();
	if (amInterrupted()) {
	  samplesAvail.unlock();
	  break;
	}
	samplesAvail.wait();

	if (amInterrupted()) {
	  samplesAvail.unlock();
	  break;
	}

	// cerr << "samples.size=" << samples.size() << endl;
	SortedSampleSet::const_reverse_iterator latest = samples.rbegin();
	if (latest == samples.rend()) continue;	// empty
	dsm_time_t tt = (*latest)->getTimeTag() - buflenInMillisec;
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

	// grab the samples before the iterator
	std::vector<const Sample*> agedsamples(rsb,rsi);

	// remove samples from sorted multiset
	samples.erase(rsb,rsi);

	// free the lock
	samplesAvail.unlock();

	// loop over the aged samples
	std::vector<const Sample *>::const_iterator si;
	for (si = agedsamples.begin(); si < agedsamples.end(); ++si) {
	    const Sample *s = *si;
	    distribute(s);
	    s->freeReference();
	}
	/*
	cerr << getFullName() << " agedsamples.size=" <<
		agedsamples.size() << endl;
	*/
    }
    return RUN_OK;
}

void SampleSorter::interrupt() {
    // cerr << "SampleSorter::interrupt" << endl;
    atdUtil::Synchronized autosync(samplesAvail);
    Thread::interrupt();
    samplesAvail.signal();
    // cerr << "SampleSorter::interrupt done" << endl;
}

/**
 * flush all samples from buffer, distributing them to SampleClients.
 */
void SampleSorter::flush() throw (atdUtil::IOException)
{
    samplesAvail.lock();
    SortedSampleSet tmpset = samples;
    samples.clear();
    samplesAvail.unlock();

    SortedSampleSet::const_iterator si;
    for (si = tmpset.begin(); si != tmpset.end(); ++si) {
	const Sample *s = *si;
	distribute(s);
	s->freeReference();
    }
}

bool SampleSorter::receive(const Sample *s) throw()
{
    atdUtil::Synchronized autosync(samplesAvail);
    samples.insert(samples.end(),s);
    s->holdReference();

    // signal the handler thread every threadSignalFactor
    // number of samples
    sampleCtr = (++sampleCtr % threadSignalFactor);
    if (!sampleCtr) samplesAvail.signal();
    return true;
}

