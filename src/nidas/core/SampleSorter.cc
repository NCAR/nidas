/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SampleSorter.h>
#include <nidas/core/DSMTime.h>
#include <nidas/core/Looper.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <vector>
#include <limits> // numeric_limits<>

// #include <unistd.h>	// for sleep
#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleSorter::SampleSorter(const string& name) :
    Thread(name),sorterLengthUsec(250*USECS_PER_MSEC),
    heapMax(10000000),heapSize(0),heapBlock(false),discardedSamples(0),
    discardWarningCount(1000),doFlush(false),flushed(false)
{
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
}

SampleSorter::~SampleSorter()
{
    // make sure thread is not running.
    if (isRunning()) interrupt();
    if (!isJoined()) {
        try {
#ifdef DEBUG
            cerr << "~SampleSorter, joining" << endl;
#endif
            join();
        }
        catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_WARNING,"%s",e.what());
        }
    }

    sampleSetCond.lock();
    SortedSampleSet tmpset = samples;
    samples.clear();
    sampleSetCond.unlock();
    // Looper::getInstance()->removeClient(this);

#ifdef DEBUG
    cerr << "freeing reference on samples" << endl;
#endif
    SortedSampleSet::const_iterator si;
    for (si = tmpset.begin(); si != tmpset.end(); ++si) {
	const Sample *s = *si;
	s->freeReference();
    }
}

void SampleSorter::addSampleTag(const SampleTag* tag,SampleClient* client)
    	throw(n_u::InvalidParameterException)
{
    clientMapLock.lock();
    map<dsm_sample_id_t,SampleClientList>::iterator ci =
    	clientsBySampleId.find(tag->getId());

    if (ci == clientsBySampleId.end()) {
	SampleClientList clients;
	clients.add(client);
	clientsBySampleId[tag->getId()] = clients;
    }
    else ci->second.add(client);
    clientMapLock.unlock();
}
/**
 * Thread function.
 */
int SampleSorter::run() throw(n_u::Exception)
{

    // Looper::getInstance()->addClient(this,sorterLengthUsec/2/USECS_PER_MSEC);

    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s: sorterLengthUsec=%d",
	getName().c_str(),sorterLengthUsec);

    sampleSetCond.lock();
#ifdef DEBUG
    dsm_time_t tlast;
#endif
    for (;;) {

	if (amInterrupted()) break;

	if (doFlush && samples.size() == 1) {
	    flush();
	    flushed = true;
	    doFlush = false;
	}

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
	    	n_u::Logger::getInstance()->log(LOG_NOTICE,
		    "increased heapMax to %d, # of samples=%d",
		    heapMax,size());
	    }
	    heapCond.unlock();
	    sampleSetCond.wait();
	    continue;
	}

	// grab the samples before the iterator
	std::vector<const Sample*> agedsamples(rsb,rsi);

#ifdef DEBUG
	n_u::UTime now;
	cerr << getFullName() << now.format(true,"%H:%M:%S.%6f") <<
		" agedsamples.size=" << agedsamples.size() << endl;
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
	    ssum += s->getDataByteLength() + s->getHeaderLength();

#ifdef DEBUG
	    dsm_time_t tsamp = s->getTimeTag();
	    if (tsamp < tlast) {
		cerr << "tsamp=" << n_u::UTime(tsamp).format(true,"%Y %m %d %H:%M:%S.%6f") <<
		    " tlast=" << n_u::UTime(tlast).format(true,"%Y %m %d %H:%M:%S.%6f") <<
                    " id=" << GET_DSM_ID(s->getId()) << ',' << GET_SHORT_ID(s->getId()) <<
                        " sorterLength=" << sorterLengthUsec/USECS_PER_MSEC << " msec"<< endl;
	    }
	    tlast = tsamp;
#endif

	    clientMapLock.lock();
	    map<dsm_sample_id_t,SampleClientList>::const_iterator ci =
		clientsBySampleId.find(s->getId());
	    if (ci != clientsBySampleId.end()) {
	        SampleClientList tmp(ci->second);
		clientMapLock.unlock();
		list<SampleClient*>::const_iterator li = tmp.begin();
		for ( ; li != tmp.end(); ++li) (*li)->receive(s);
	    }
	    else clientMapLock.unlock();

	    distribute(s);
	}
	heapDecrement(ssum);

	sampleSetCond.lock();

    }
    sampleSetCond.unlock();
    // Looper::getInstance()->removeClient(this);
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

    // If we only did a signal without locking,
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
void SampleSorter::finish() throw()
{
    SampleT<char>* eofSample = getSample<char>(0);
    numeric_limits<long long> ll;
    eofSample->setTimeTag(ll.max());
    eofSample->setId(0);

    sampleSetCond.lock();
    flushed = false;
    doFlush = true;
    samples.insert(samples.end(),eofSample);
    sampleSetCond.unlock();
    sampleSetCond.signal();


    for (int i = 1; ; i++) {
	struct timespec ns = {0, NSECS_PER_SEC / 10};
	nanosleep(&ns,0);
	sampleSetCond.lock();
	if (!(i % 20))
	    n_u::Logger::getInstance()->log(LOG_NOTICE,
		"waiting for buffer to empty, size=%d",
			samples.size());
	if (samples.size() == 1 && flushed) break;
	sampleSetCond.unlock();
    }
    sampleSetCond.unlock();
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
	    	n_u::Logger::getInstance()->log(LOG_WARNING,
	"%d discarded samples because heapSize(%d) + sampleSize(%d) is > than heapMax(%d)",
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

    s->holdReference();
    sampleSetCond.lock();
    samples.insert(samples.end(),s);
    sampleSetCond.unlock();

#ifndef USE_LOOPER
    sampleSetCond.signal();
#endif

    return true;
}

#ifdef USE_LOOPER
void SampleSorter::looperNotify() throw()
{
#ifdef DEBUG
    n_u::UTime now;
    cerr << "looperNotify: " << now.format(true,"%H:%M:%S.%6f") << endl;
#endif
    sampleSetCond.signal();
}
#endif
