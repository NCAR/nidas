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
    _heapSize(0),_heapBlock(false),
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
            n_u::Logger::getInstance()->log(LOG_WARNING,"%s",e.what());
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

// #define USE_SAMPLE_SET_COND_SIGNAL
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

    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s: sorterLengthUsec=%d, heapMax=%d, heapBlock=%d",
	getName().c_str(),_sorterLengthUsec,_heapMax,_heapBlock);

#ifdef DEBUG
    dsm_time_t tlast;
#endif

    _sampleSetCond.lock();

    for (;;) {

	if (amInterrupted()) break;

        size_t nsamp = _samples.size();

        // if _doFinish and only one sample, it must be the dummy
        // sample with a into-the-future timetag.
	if (_doFinish && nsamp == 1) {
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

	if (nsamp == 0) {	// no samples, wait
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
	    // If no aged samples but we're at the heap limit,
	    // then we need to extend the limit, because it isn't
            // big enough for the current data rate (bytes/second).
            // The reason for doing this heap checking is to detect
            // if this sample consumer thread has gotten behind the threads
            // which are putting samples in this sorter. This would
            // happen, for example if the SampleClients of this sorter
            // are being blocked when doing I/O. In that case
            // we don't expand the heap, because it probably wouldn't
            // help the system keep up.
            // If there are no aged samples then this consumer
            // thread is not behind, and instead the amount of data
            // in the sorter:
            //      bytes/second * sorterLength
            // is greater than the current value of heapMax,
            // so expand heapMax a bit.
            //
            // The only way that heapSize can be > heapMax is if
            // heapBlock is true, meaning that the sample generator
            // waits until heapSize is < heapMax before putting 
            // in more samples.
	    _heapCond.lock();
	    if (_heapSize > _heapMax) {
                while (_heapSize > _heapMax) {
                    _heapMax += _heapMax / 8;
                    n_u::Logger::getInstance()->log(LOG_NOTICE,
                        "increased heapMax to %d, # of samples=%d",
                        _heapMax,size());
                }
		_heapCond.signal();
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
    // the interrupt could be missed.

    Thread::interrupt();
    _sampleSetCond.unlock();
#ifdef USE_SAMPLE_SET_COND_SIGNAL
    _sampleSetCond.signal();
#endif
}

// We've removed some samples from the heap. Decrement heapSize
// and signal waiting threads if the heapSize
// has shrunk to less than heapMax bytes.
void inline SampleSorter::heapDecrement(size_t bytes)
{
    _heapCond.lock();
    if (!_heapBlock) _heapSize -= bytes;
    else {
	if (_heapSize > _heapMax) {	// SampleSource must be waiting
	    _heapSize -= bytes;
	    if (_heapSize <= _heapMax) {
		// cerr << "signalling heap waiters, heapSize=" << heapSize << endl;
		_heapCond.signal();
	    }
	}
	else _heapSize -= bytes;
    }
    _heapCond.unlock();
}

/**
 * distributing all samples to SampleClients.
 */
void SampleSorter::finish() throw()
{
    _sampleSetCond.lock();

    // check if finish already requested.
    if (_finished || _doFinish) {
        _sampleSetCond.unlock();
        return;
    }

    ILOG(("waiting for ") << _samples.size() << ' ' <<
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
	    ILOG(("waiting for SampleSorter to empty, size=%d, nwait=%d",
			_samples.size(),i));
	if (_finished) break;
	_sampleSetCond.unlock();
    }
    _sampleSetCond.unlock();
    ILOG(((_source.getRawSampleSource() ? "raw" : "processed")) <<
        " samples drained from SampleSorter");

}

bool SampleSorter::receive(const Sample *s) throw()
{
    size_t slen = s->getDataByteLength() + s->getHeaderLength();

    if (_realTime) {
        dsm_time_t samptt = s->getTimeTag();
        dsm_time_t systt = getSystemTime();
        if (samptt > systt + USECS_PER_SEC / 4) {
	    if (!(_realTimeFutureSamples++ % _discardWarningCount))
	    	WLOG(("discarded sample with timetag in future by %f secs. time: ",
                    (float)(samptt - systt) / USECS_PER_SEC) <<
                    n_u::UTime(samptt).format(true,"%Y %b %d %H:%M:%S.%3f") <<
                    " id=" << GET_DSM_ID(s->getId()) << ',' << GET_SPS_ID(s->getId()) <<
                    " total future discards=" << _realTimeFutureSamples);
	    return false;
        }
    }

    // Check if the heapSize will exceed heapMax
    _heapCond.lock();
    if (!_heapBlock) {
        // Real-time behaviour, discard samples rather than blocking threads
        if (_heapSize + slen > _heapMax) {
	    _heapCond.unlock();
	    if (!(_discardedSamples++ % _discardWarningCount))
	    	n_u::Logger::getInstance()->log(LOG_WARNING,
	"%d discarded samples because heapSize(%d) + sampleSize(%d) is > than heapMax(%d)",
		_discardedSamples,_heapSize,slen,_heapMax);
	    return false;
	}
	_heapSize += slen;
    }
    else {
        // Post-processing behavour: this thread will block until heap
        // gets smaller than _heapMax
	_heapSize += slen;
	// if heapMax will be exceeded, then wait until heapSize comes down
	while (_heapSize > _heapMax) {
	    // Wait until we've distributed enough samples
	    // cerr << "waiting on heap condition, heapSize=" << heapSize << endl;
#ifdef USE_SAMPLE_SET_COND_SIGNAL
	    _sampleSetCond.signal();
#endif
	    _heapCond.wait();
	    // cerr << "received heap signal, heapSize=" << heapSize << endl;
	}
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

