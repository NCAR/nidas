/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <vector>

#include <SampleSorter.h>
#include <DSMTime.h>

#include <unistd.h>	// for sleep
#include <iostream>

using namespace dsm;
using namespace std;

SampleSorter::SampleSorter(int buflenInMilliSec_a) :
    Thread("SampleSorter"),buflenInMillisec(buflenInMilliSec_a),
    samplesAvail("samplesAvail"),threadSignalFactor(10)
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

	dummy.setTimeTag((getCurrentTimeInMillis() - buflenInMillisec)
		% MSECS_PER_DAY);
	// std::cout << "awoke in thread func, samples.size()=" << samples.size() <<
	//	" tt=" << dummy.tt << std::endl;

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
	std::vector<const Sample *>::iterator iv;
	for (iv = agedsamples.begin(); iv < agedsamples.end(); iv++) {
	    const Sample *s = *iv;
	    distribute(s);
	}
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

bool SampleSorter::receive(const Sample *s)
	throw(SampleParseException, atdUtil::IOException)
{
    static int nsamp = 0;
    atdUtil::Synchronized autosync(samplesAvail);
    samples.insert(samples.end(),s);
    s->holdReference();

    // signal the handler thread every threadSignalFactor
    // number of samples
    nsamp = (++nsamp % threadSignalFactor);
    if (!nsamp) samplesAvail.signal();
    return true;
}

