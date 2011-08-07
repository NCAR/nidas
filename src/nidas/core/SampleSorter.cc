/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 * This SampleSorter is implemented using a SortedSampleSet and a thread.
 * As a SampleClient, it implements a receive() method.  As samples are received
 * they are placed in the SortedSampleSet, which sorts them by timetag.
 * The loop in the run method of the separate thread then wakes periodically
 * and checks if any samples have aged off in the SortedSampleSet. It looks at the
 * time of the most recent sample in the SortedSampleSet, and extracts the samples
 * whose timetags are earlier than (mostRecent - sorterLength). These samples
 * are then sent on to the SampleClients of the SampleSorter. This is fairly simple.
 * The complication is that SampleClient code running in the second thread
 * may run slower than the thread feeding samples into the SortedSampleSet, and the
 * SortedSampleSet could grow without bound.  To prevent that a heapMax is
 * imposed, in one of two ways, depending on whether this code
 * is sorting samples acquired in real-time, or is post-processing.
 *
 * If the SampleSorter is running during real-time acquisition, and a sample
 * is received which will exceed heapMax, then it is discarded with a warning.
 * This, of course is not a desireable situation. It can be prevented by chosing
 * a large enough heapMax to get past short periods of system slowdown,
 * by chosing an appropriate priority for the threads, and investigating
 * the reasons for slowdown. Choosing a heapMax so large that the system
 * starts swapping will probably not improve the situation.
 *
 * If post-processing, and a sample is received which, when added to
 * the SortedSampleSet, will result in the heapMax being exceed, then the thread
 * which is calling receive is blocked by waiting on a condition variable.
 * That condition variable is signaled by the second thread when the size
 * of the samples in the SortedSampleSet falls below 50% of heapMax.
 *
 * The value of heapMax is dynamically increased by 1/2 in the loop of the run
 * method if the number of bytes in the SortedSampleSet has reached heapMax, but
 * there are no aged samples.
 *
 ********************************************************************
 */

#include <nidas/core/SampleSorter.h>
#include <nidas/core/DSMTime.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <vector>
#include <limits> // numeric_limits<>

// #include <unistd.h>	// for sleep
#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleSorter::SampleSorter(const string& name,bool raw) :
    SampleThread(name),_source(raw),
    _sorterLengthUsec(250*USECS_PER_MSEC),
#ifdef NIDAS_EMBEDDED
    _heapMax(5000000),
#else
    _heapMax(50000000),
#endif
    _heapSize(0),_heapBlock(false),_heapExceeded(false),
    _discardedSamples(0),_realTimeFutureSamples(0),_discardWarningCount(1000),
    _doFinish(false),_finished(false),
    _realTime(false)
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
            WLOG(("%s",e.what()));
        }
    }

    _sampleSetCond.lock();
    SortedSampleSet tmpset = _samples;
    _samples.clear();
    _sampleSetCond.unlock();

#ifdef DEBUG
    cerr << "freeing reference on samples" << endl;
#endif
    SortedSampleSet::const_iterator si;
    for (si = tmpset.begin(); si != tmpset.end(); ++si) {
	const Sample *s = *si;
	s->freeReference();
    }
}

/**
 * Thread function.
 */
int SampleSorter::run() throw(n_u::Exception)
{

// #define TEST_CPU_TIME
#ifdef TEST_CPU_TIME
    unsigned int ntotal = 0;
    unsigned int smax=0,smin=INT_MAX,savg=0,nloop=0;
#endif

#define USE_SAMPLE_SET_COND_SIGNAL
//
    /* testing on a viper with 10 sonics, each at 60 Hz. Sorting of raw samples.
     * Linux tunnel 2.6.16.28-arcom1-2-viper #1 PREEMPT Wed Sep 16 17:04:19 MDT 2009 armv5tel unknown
     * samples. SampleSorter running at realtime FIFO priority of 40.
     *
     * rawSorterLength=1 sec
     *
     * Ran for roughly 5 minutes of data (5 * 60 seconds * 60 Hz * 10 sonics) = 180000 samples
     * Takes longer than 5 minutes because of the time spent initializing sonics.
     *
     *********************************************************************************
     * receive() method sends a condition signal on every sample.
     * run method waits on signal if no samples.
     * 180000 sample test
     * nloop=129801 smax=25 smin=1 savg=1.38675
     *
     * real    5m35.048s
     * user    0m12.440s
     * sys     0m21.140s
     *
     * CPU time is high perhaps because of excessive checking for aged samples
     *
     *********************************************************************************
     * If no samples or aged samples, sleep for 1/100th second and check again. No condition signals:
     *
     * 180000 sample test
     * nloop=16127 smax=18 smin=1 savg=11.1615
     *
     * real    5m38.453s
     * user    0m3.570s
     * sys     0m5.610s
     *
     * Those are good numbers! Not much slower than no sorting:
     * nloop=15928 smax=17 smin=1 savg=11.3014
     * 
     * real    5m28.118s
     * user    0m2.490s
     * sys     0m4.370s
     */

#ifndef USE_SAMPLE_SET_COND_SIGNAL
    struct timespec sleepr = { 0, NSECS_PER_SEC / 100 };
#endif

    ILOG(("%s: sorterLengthUsec=%d, heapMax=%d, heapBlock=%d",
	getName().c_str(),_sorterLengthUsec,_heapMax,_heapBlock));

#ifdef DEBUG
    dsm_time_t tlast;
#endif

    _sampleSetCond.lock();

    for (;;) {

	if (amInterrupted()) break;

        size_t nsamp = _samples.size();

        // if _doFinish and only one sample, it must be the dummy
        // sample with a into-the-future timetag.
	if (_doFinish) {
            if (nsamp == 1) {
#ifdef DEBUG
                cerr << "SampleSorter calling flush, _source type=" << 
                    (_source.getRawSampleSource() ? "raw" : "proc") << 
                    " client count=" << _source.getClientCount() << endl;
#endif
                _sampleSetCond.unlock();
                // calls finish() on all sample clients.
                flush();
                _sampleSetCond.lock();
                _finished = true;
                _doFinish = false;
                const Sample *s = *_samples.begin();
                s->freeReference();
                _samples.clear();
                nsamp = 0;
                continue;
            }
        }
        else if (nsamp <= SCREEN_NUM_BAD_TIME_TAGS) {	// not enough samples, wait
#ifdef USE_SAMPLE_SET_COND_SIGNAL
            _sampleSetCond.wait();
#else
            _sampleSetCond.unlock();
            ::nanosleep(&sleepr,0);
            _sampleSetCond.lock();
#endif
	    continue;
	}

	SortedSampleSet::const_reverse_iterator latest = _samples.rbegin();

        // back up over SCREEN_NUM_BAD_TIME_TAGS number of latest samples before
        // using a sample time to use for the age off.
        for (unsigned int i = 0; i < SCREEN_NUM_BAD_TIME_TAGS && !_doFinish; i++) latest++;

        // age-off samples with timetags before this
        dsm_time_t tt = (*latest)->getTimeTag() - _sorterLengthUsec;              
	_dummy.setTimeTag(tt);
	// cerr << "tt=" << tt << endl;
	/*
	cerr << getFullName() << " samples.size=" <<
		_samples.size() <<
		" tt=" << _dummy.getTimeTag() << std::endl;
        */

	SortedSampleSet::const_iterator rsb = _samples.begin();

	// get iterator pointing at first sample not less than dummy
	SortedSampleSet::const_iterator rsi = _samples.lower_bound(&_dummy);

	if (rsi == rsb) { // no aged samples
	    // If no aged samples, but we're at the heap limit,
	    // then we need to extend the limit, because it isn't
            // big enough for the current data rate (bytes/second).
	    _heapCond.lock();
	    if (_heapExceeded) {
                _heapMax += _heapMax / 2;
                NLOG(("") << getName() << ": increased heapMax to " << _heapMax <<
                    ", current # of samples=" << size());
		_heapCond.signal();
                _heapExceeded = false;
            }
	    _heapCond.unlock();
#ifdef USE_SAMPLE_SET_COND_SIGNAL
            _sampleSetCond.wait();
#else
            _sampleSetCond.unlock();
            ::nanosleep(&sleepr,0);
            _sampleSetCond.lock();
#endif
	    continue;
	}

	// grab the samples before the iterator
	std::vector<const Sample*> agedsamples(rsb,rsi);

#ifdef TEST_CPU_TIME
        nsamp = agedsamples.size();
        smax = std::max(smax,nsamp);
        smin = std::min(smin,nsamp);
        savg += nsamp;
        nloop++;
#endif

#ifdef DEBUG
	n_u::UTime now;
	cerr << getFullName() << now.format(true,"%H:%M:%S.%6f") <<
		" agedsamples.size=" << agedsamples.size() << endl;
#endif

	// remove samples from sorted multiset
	_samples.erase(rsb,rsi);

	// free the lock
	_sampleSetCond.unlock();

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
                        " sorterLength=" << _sorterLengthUsec/USECS_PER_MSEC << " msec"<< endl;
	    }
	    tlast = tsamp;
#endif
#ifdef TEST_CPU_TIME
            if (ntotal++ == 10 * 60 * 60 * 5) {
                cerr << "nloop=" << nloop << " smax=" << smax << " smin=" << smin << " savg=" << (double)savg/nloop << endl;
                exit(1);
            }
#endif
            _source.distribute(s);
	}
	heapDecrement(ssum);

	_sampleSetCond.lock();
    }
    _sampleSetCond.unlock();
    return RUN_OK;
}

void SampleSorter::interrupt()
{
    _sampleSetCond.lock();

    // After setting this lock, we know that the
    // consumer thread is either:
    //	* waiting on sampleSetCond,
    // 	* flushing clients
    // 	* distributing samples or,
    //	* hasn't started looping,
    // since those are the only times sampleSetCond is unlocked.

    // If we only did a signal without locking,
    // the interrupt could be missed if this interrupt() and
    // signal() happened after the consumer thread checked
    // amInterrupted() but before the wait. To prevent this the
    // consumer thread locks _sampleSetCond before the call of
    // amInterrupted() and keeps it locked until the wait().

    Thread::interrupt();
    _sampleSetCond.unlock();
#ifdef USE_SAMPLE_SET_COND_SIGNAL
    _sampleSetCond.signal();
#endif

    // In case a thread calling receive is waiting on heapCond.
    _heapCond.lock();
    // make sure that it won't block on the heapMax again
    // could just set heapSize to 0 I suppose.
    if (_heapExceeded || _heapSize > _heapMax /2) _heapMax += _heapMax / 2;
    _heapCond.signal();
    _heapCond.unlock();
}

// We've removed some samples from the heap. Decrement heapSize
// and signal waiting threads if the heapSize has shrunk enough.
void SampleSorter::heapDecrement(size_t bytes)
{
    _heapCond.lock();
    if (!_heapBlock) _heapSize -= bytes;
    else {
	if (_heapExceeded) {	// receive() method is waiting on _heapCond
	    _heapSize -= bytes;
            // To reduce trashing, wait until heap has decreased to 50% of _heapMax
            // before signalling a waiting thread.
            // Note there is a possibility that more than 50% of the heap
            // is really needed to hold a sorter length's amount of samples.
            // That situation is caught by the run method when there are no
            // aged samples.
	    if (_heapSize < _heapMax/2) {
		// cerr << "signalling heap waiters, heapSize=" << heapSize << endl;
                DLOG(("") << getName() << ": heap(" << _heapSize << ") < 1/2 * max(" << _heapMax << "), resuming");
		_heapCond.signal();
                _heapExceeded = false;
	    }
	}
	else _heapSize -= bytes;
    }
    _heapCond.unlock();
}

/**
 * distribute all samples to SampleClients.
 */
void SampleSorter::finish() throw()
{
    _sampleSetCond.lock();

    // check if finish already requested.
    if (_finished || _doFinish) {
        _sampleSetCond.unlock();
        return;
    }

    DLOG(("waiting for ") << _samples.size() << ' ' <<
        (_source.getRawSampleSource() ? "raw" : "processed") <<
        " samples to drain from SampleSorter");

    SampleT<char>* eofSample = getSample<char>(0);
    numeric_limits<long long> ll;
    eofSample->setTimeTag(ll.max());
    eofSample->setId(0);

    _finished = false;
    _doFinish = true;
    _samples.insert(_samples.end(),eofSample);
    _sampleSetCond.unlock();

#ifdef USE_SAMPLE_SET_COND_SIGNAL
    _sampleSetCond.signal();
#endif

    for (int i = 1; ; i++) {
	struct timespec ns = {0, NSECS_PER_SEC / 10};
	nanosleep(&ns,0);
	_sampleSetCond.lock();
	if (!(i % 200))
	    DLOG(("waiting for ") << getName() <<
		" to empty, size=" << _samples.size() <<
		",  nwait=" << i);
	if (_finished) break;
	_sampleSetCond.unlock();
    }
    _sampleSetCond.unlock();
    DLOG(((_source.getRawSampleSource() ? "raw" : "processed")) <<
        " samples drained from SampleSorter");

}

bool SampleSorter::receive(const Sample *s) throw()
{
    unsigned int slen = s->getDataByteLength() + s->getHeaderLength();

    if (_realTime) {
        dsm_time_t samptt = s->getTimeTag();
        dsm_time_t systt = getSystemTime();
	// On DSMs with samples which are timetagged by an IRIG, the IRIG clock
	// can be off if it doesn't have a lock
        if (samptt > systt + USECS_PER_SEC * 2) {
	    if (!(_realTimeFutureSamples++ % _discardWarningCount))
	    	WLOG(("sample with timetag in future by %f secs. time: ",
                    (float)(samptt - systt) / USECS_PER_SEC) <<
                    n_u::UTime(samptt).format(true,"%Y %b %d %H:%M:%S.%3f") <<
                    " id=" << GET_DSM_ID(s->getId()) << ',' << GET_SPS_ID(s->getId()) <<
                    " total future samples=" << _realTimeFutureSamples);
	    return false;
        }
    }

    // Check if the heapSize will exceed heapMax
    _heapCond.lock();
    if (!_heapBlock) {
        // Real-time behaviour, discard samples rather than blocking threads
        if (_heapSize + slen > _heapMax) {
            _heapExceeded = true;
	    _heapCond.unlock();
	    if (!(_discardedSamples++ % _discardWarningCount))
	    	WLOG(("%d discarded samples because heapSize(%d) + sampleSize(%d) is > than heapMax(%d)",
		_discardedSamples,_heapSize,slen,_heapMax));
	    return false;
	}
	_heapSize += slen;
        _heapExceeded = false;
    }
    else {
        // Post-processing: this thread will block until heap
        // gets smaller than _heapMax
	_heapSize += slen;
	// if heapMax will be exceeded, then wait until heapSize comes down
	while (_heapSize > _heapMax) {
            // We want to avoid a deadlock where the consumer thread is waiting
            // in SampleSorter::run because it has no aged samples,
            // while the producer thread is waiting in this receive() method
            // because the heap is exceeded. Setting/checking
            // _heapExceeded with _heapCond locked should do the trick.
            _heapExceeded = true;
            DLOG(("") << getName() << ": heap(" << _heapSize <<
                ") > max(" << _heapMax << "), waiting");
#ifdef USE_SAMPLE_SET_COND_SIGNAL
	    _sampleSetCond.signal();
#endif
	    // Wait until consumer thread has distributed enough samples
	    _heapCond.wait();
	    // cerr << "received heap signal, heapSize=" << heapSize << endl;
	}
        _heapExceeded = false;
    }
    _heapCond.unlock();

    s->holdReference();
    _sampleSetCond.lock();
    _samples.insert(_samples.end(),s);
    _sampleSetCond.unlock();

#ifdef USE_SAMPLE_SET_COND_SIGNAL
    _sampleSetCond.signal();
#endif

    return true;
}

