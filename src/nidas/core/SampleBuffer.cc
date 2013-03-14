// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SampleBuffer.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <vector>
#include <limits> // numeric_limits<>

// #include <unistd.h>	// for sleep
#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleBuffer::SampleBuffer(const string& name,bool raw) :
    SampleThread(name),
#ifdef USE_DEQUE
    _sampleBuf(),
#else
    _sampleBufs(),_inserterBuf(),_consumerBuf(),
#endif
    _source(raw),_sampleBufCond(),_flushCond(),
#ifdef NIDAS_EMBEDDED
    _heapMax(5000000),
#else
    _heapMax(50000000),
#endif
    _heapSize(0),_heapBlock(false),_heapCond(),
    _discardedSamples(0),_realTimeFutureSamples(0),_discardWarningCount(1000),
    _doFlush(false),_flushed(true),
    _realTime(false)
{
#ifndef USE_DEQUE
    _inserterBuf = &_sampleBufs[0];
    _consumerBuf = &_sampleBufs[1];
#endif
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
}

SampleBuffer::~SampleBuffer()
{
    // make sure thread is not running.
    if (isRunning()) interrupt();
    if (!isJoined()) {
        try {
#ifdef DEBUG
            cerr << "~SampleBuffer, joining" << endl;
#endif
            join();
        }
        catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_WARNING,"%s",e.what());
        }
    }

}

size_t SampleBuffer::size() const
{
    n_u::Autolock alock (_sampleBufCond);
    return sizeNoLock();
}

size_t SampleBuffer::sizeNoLock() const
{
#ifdef USE_DEQUE
    return _sampleBuf.size();
#else
    return _inserterBuf->size() + _consumerBuf->size();
#endif
}

bool SampleBuffer::emptyNoLock() const
{
#ifdef USE_DEQUE
    return _sampleBuf.empty();
#else
    return _inserterBuf->empty() && _consumerBuf->empty();
#endif
}

/**
 * Thread function.
 */
int SampleBuffer::run() throw(n_u::Exception)
{
#ifdef DEBUG
    dsm_time_t tlast;
#endif

// #define TEST_CPU_TIME
#ifdef TEST_CPU_TIME
    unsigned int ntotal = 0;
    unsigned int smax=0,smin=INT_MAX,savg=0,nloop=0;
#endif

    /* testing on a viper with 10 sonics, each at 60 Hz.
     * Linux tunnel 2.6.16.28-arcom1-2-viper #1 PREEMPT Wed Sep 16 17:04:19 MDT 2009 armv5tel unknown
     * Sorting raw samples. SampleBuffer running at realtime FIFO priority of 40.
     *
     * Ran for roughly 5 minutes of data (5 * 60 seconds * 60 Hz * 10 sonics) = 180000 samples
     * Takes longer than 5 minutes because of the time spent initializing sonics.
     *
     *********************************************************************************
     * receive() method sends a condition signal on every sample.
     * run method waits on signal if not samples.
     *
     * 180000 sample test
     * nloop=169595 smax=62 smin=1 savg=1.06136
     *
     * real    5m33.702s
     * user    0m5.660s
     * sys     0m10.960s
     *
     *********************************************************************************
     *
     *********************************************************************************
     * If no samples, sleep for 1/10th second and check again. No condition signals:
     *
     * 180000 sample test
     * nloop=2754 smax=79 smin=1 savg=65.366
     *
     * real    5m36.164s
     * user    0m13.100s
     * sys     0m15.080s
     *
     * Seems that it takes a disproportionate amount of time to make copies of
     * and clear larger lists.
     *
     * Changed to sleep for 1/100th second if no samples:
     * nloop=15928 smax=17 smin=1 savg=11.3014
     *
     * real    5m28.118s
     * user    0m2.490s
     * sys     0m4.370s
     *
     * This is very similar to 1/100th second sleep on every loop. Seems
     * that after handling a list of samples the next loop probably finds
     * none, and sleeps. Also agrees with result that large lists have a penalty.
     * Changed to a vector, and saw 
     * nloop=16093 smax=17 smin=1 savg=11.1858
     *
     * real    5m31.490s
     * user    0m2.380s
     * sys     0m4.230s
     *
     * 1/10th second sleep:
     * nloop=2687 smax=81 smin=1 savg=67.0022
     *
     * real    5m28.076s
     * user    0m13.250s
     * sys     0m15.530s
     * no better with vector than with list
     *
     *********************************************************************************
     *
     *********************************************************************************
     * Sleep for 1/100th second every time through loop.
     *
     * 180000 sample test:
     * nloop=16292 smax=30 smin=1 savg=11.0492
     *
     * real    5m39.529s
     * user    0m2.490s
     * sys     0m4.350s
     *
     * In theory, each loop should handle about 10*60/100 = 6 samples.
     * Instead it averaged to 11 samples
     *********************************************************************************
     *
     * Testing on C130, Oct 8, 2009, pre-PLOWS, dsm320 with 2 A2Ds, each with 500 Hz
     * output samples, not reading SYNC word from A2D fifo.
     * Linux dsm320 2.6.16.28-arcom1-2-viper #1 PREEMPT Tue Oct 6 10:29:36 MDT 2009 armv5tel unknown
     *
     * 1000 * 60 * 5  samples (around 5 minutes)
     * Send cond signal in each input sample. Double buffering with 2 vectors.
     *
     * nloop=24974 smax=60 smin=1 savg=12.0131
     *
     * real    4m20.528s
     * user    0m41.000s
     * sys     0m20.500s
     *
     * nloop=24895 smax=59 smin=1 savg=12.0525
     * real    4m18.994s
     * user    0m40.260s
     * sys     0m20.820s
     *
     * 1000 * 60 * 5  samples (around 5 minutes)
     * Send cond signal in each input sample. Using deque rather than double vectors.
     *
     * nloop=24267 smax=1045 smin=1 savg=12.2341
     * real    4m18.945s
     * user    0m38.340s
     * sys     0m21.130s

     * nloop=24747 smax=60 smin=1 savg=12.1219
     * real    4m20.157s
     * user    0m39.410s
     * sys     0m22.720s
     *
     * Not much difference between deque and double vectors.
     *
     * Test of nanosleep of 10 msec instead of condition variable signals
     * did not go well on dsm320, many skipped samples in a2d driver.
     * So, stick with sending condition signals.
     *********************************************************************************
     */
#define USE_SAMPLE_SET_COND_SIGNAL
#ifndef USE_SAMPLE_SET_COND_SIGNAL
    struct timespec sleepr = { 0, NSECS_PER_SEC / 100 };
#endif

    _sampleBufCond.lock();

    for (;;) {


	if (amInterrupted()) break;

#ifdef USE_DEQUE
        size_t nsamp = _sampleBuf.size();
#else
        assert(_consumerBuf->empty());
        size_t nsamp = _inserterBuf->size();
#endif

	if (nsamp == 0) {	// no samples, wait
            _flushed = true;
            if (_doFlush ) {
                _flushCond.lock();
                _flushCond.unlock();
                _flushCond.broadcast();
                _doFlush = false;
            }
            

#ifdef USE_SAMPLE_SET_COND_SIGNAL
            _sampleBufCond.wait();
#else
            _sampleBufCond.unlock();
            ::nanosleep(&sleepr,0);
            _sampleBufCond.lock();
#endif

            continue;
	}

#ifdef TEST_CPU_TIME
	smax = std::max(smax,nsamp);
	smin = std::min(smin,nsamp);
	savg += nsamp;
        nloop++;
#endif

#ifndef USE_DEQUE
        // switch buffers
        std::vector<const Sample*>* tmpPtr = _consumerBuf;
        _consumerBuf = _inserterBuf;
        _inserterBuf = tmpPtr;
        _sampleBufCond.unlock();
#endif

	// loop over the buffered samples
	size_t ssum = 0;
#ifdef USE_DEQUE
        
        // has at least one sample because of the nsamp check above
        do {
	    const Sample *s = _sampleBuf.front();
	    _sampleBuf.pop_front();

            _sampleBufCond.unlock();

	    ssum += s->getDataByteLength() + s->getHeaderLength();
	    _source.distribute(s);
#ifdef TEST_CPU_TIME
            if (ntotal++ == 1000 * 60 * 5) {
		cerr << "nloop=" << nloop << " smax=" << smax << " smin=" << smin << " savg=" << (double)savg/nloop << endl;
                exit(1);
            }
#endif
            _sampleBufCond.lock();
        } while(!_sampleBuf.empty());
            
#else
        // _sampleBufCond is unlocked, since the receiver thread
        // does not touch _consumerBuf.
	std::vector<const Sample *>::const_iterator si;
	for (si = _consumerBuf->begin(); si != _consumerBuf->end(); ++si) {
	    const Sample *s = *si;
	    ssum += s->getDataByteLength() + s->getHeaderLength();
	    _source.distribute(s);
#ifdef TEST_CPU_TIME
            if (ntotal++ == 1000 * 60 * 5) {
		cerr << "nloop=" << nloop << " smax=" << smax << " smin=" << smin << " savg=" << (double)savg/nloop << endl;
                exit(1);
            }
#endif
	}
        _consumerBuf->clear();
#endif
	heapDecrement(ssum);

#ifndef USE_DEQUE
	_sampleBufCond.lock();
#endif
    }

#ifndef USE_DEQUE
    assert(_consumerBuf->empty());
#endif

    // warn if remaining samples
    if (!emptyNoLock())
        WLOG(("SampleBuffer (%s) run method exiting, _samples.size()=%zu",
            (_source.getRawSampleSource() ? "raw" : "processed"),sizeNoLock()));


#ifdef USE_DEQUE
    deque<const Sample*>::const_iterator di = _sampleBuf.begin();
    for ( ; di != _sampleBuf.end(); ++di) {
        const Sample *s = *di;
        s->freeReference();
    }
#else
    for (int i = 0; i < 2; i++) {
        vector<const Sample*>::const_iterator si;
        for (si = _sampleBufs[i].begin(); si != _sampleBufs[i].end();
            ++si) {
            const Sample *s = *si;
            s->freeReference();
        }
        _sampleBufs[i].clear();
    }
#endif
    _flushed = true;
    _sampleBufCond.unlock();

    _flushCond.lock();
    _flushCond.unlock();
    _flushCond.broadcast();

    return RUN_OK;
}

void SampleBuffer::interrupt()
{
    _sampleBufCond.lock();

    // After setting this lock, we know that the
    // consumer thread is either:
    //	* waiting on sampleBufCond,
    // 	* distributing samples or,
    //	* hasn't started looping,
    // since those are the only times sampleBufCond is unlocked.

    // If we only did a signal without locking,
    // this interrupt could be missed by the consumer
    // and it could then wait (forever) again on _sampleBufCond.

    Thread::interrupt();
    _sampleBufCond.unlock();
#ifdef USE_SAMPLE_SET_COND_SIGNAL
    _sampleBufCond.signal();
#endif
}

// We've removed some samples from the heap. Decrement heapSize
// and signal waiting threads if the heapSize
// has shrunk to less than heapMax bytes.
void SampleBuffer::heapDecrement(size_t bytes)
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


// #define DO_BUFFERING
#ifdef DO_BUFFERING
bool SampleBuffer::receive(const Sample *s) throw()
{
    size_t slen = s->getDataByteLength() + s->getHeaderLength();

    if (_realTime) {
        dsm_time_t systt = n_u::getSystemTime();
        dsm_time_t samptt = s->getTimeTag();
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
        // Real-time behavious, discard samples rather than blocking threads
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
	    // cerr << "waiting on heap condition, heapSize=" << _heapSize <<
            //   " heapMax=" << _heapMax << endl;
#ifdef USE_SAMPLE_SET_COND_SIGNAL
	    _sampleBufCond.signal();
#endif
	    _heapCond.wait();
	}
    }
    _heapCond.unlock();

    s->holdReference();
    _sampleBufCond.lock();
#ifdef USE_DEQUE
    _sampleBuf.push_back(s);
#else
    _inserterBuf->push_back(s);
#endif
    _flushed = false;
    _sampleBufCond.unlock();

#ifdef USE_SAMPLE_SET_COND_SIGNAL
    _sampleBufCond.signal();
#endif

    return true;
}

/*
 * Wait until all samples in buffer have been distributed.
 */
void SampleBuffer::flush() throw()
{

    _sampleBufCond.lock();
    // After setting this lock, we know that the
    // consumer thread is either:
    //	* waiting on sampleBufCond,
    // 	* distributing samples,
    //	* hasn't started looping, or
    //	* run method is finished
    // since those are the only times sampleBufCond is unlocked.

    // If we only did a signal without locking,
    // the setting of _doFlush could be missed before
    // the consumer side waits again.

    if (_flushed) {
        _sampleBufCond.unlock();
        return;
    }

    _doFlush = true;

    DLOG(("waiting for ") << sizeNoLock() << ' ' <<
        (_source.getRawSampleSource() ? "raw" : "processed") <<
        " samples to drain from SampleBuffer");

    _sampleBufCond.unlock();

#ifdef USE_SAMPLE_SET_COND_SIGNAL
    _sampleBufCond.signal();
#endif

    _flushCond.lock();
    while (!_flushed) _flushCond.wait();
    _flushCond.unlock();

    // may want to call flush() on the clients.

    DLOG(((_source.getRawSampleSource() ? "raw" : "processed")) <<
        " samples drained from SampleBuffer");
}

#else
bool SampleBuffer::receive(const Sample *s) throw()
{
    s->holdReference();

    // Even if we're not buffering, still enforce mutual exclusion,
    // so that multiple threads can stuff samples in this SampleBuffer
    // and the receive methods of downstream clients don't have to
    // worry about threading.

    _sampleBufCond.lock();
    _source.distribute(s);
    _sampleBufCond.unlock();
    return true;
}
void SampleBuffer::flush() throw()
{
    return;
}
#endif

