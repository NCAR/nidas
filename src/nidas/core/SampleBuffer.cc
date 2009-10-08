/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-05-14 11:05:29 -0600 (Thu, 14 May 2009) $

    $LastChangedRevision: 4600 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/SampleBuffer.cc $
 ********************************************************************

*/

#include <nidas/core/SampleBuffer.h>
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

SampleBuffer::SampleBuffer(const string& name,bool raw) :
    SampleThread(name),_source(raw),
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
#ifndef USE_DEQUE
    _inserterBuf = _sampleBufs[0] = new std::vector<const Sample*>;
    _consumerBuf = _sampleBufs[1] = new std::vector<const Sample*>;
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

    _sampleBufCond.lock();
#ifdef USE_DEQUE
    deque<const Sample*>::const_iterator di = _sampleBuf.begin();
    for ( ; di != _sampleBuf.end(); ++di) {
        const Sample *s = *di;
        s->freeReference();
    }
#else
    for (int i = 0; i < 2; i++) {
        vector<const Sample*>::const_iterator si;
        for (si = _sampleBufs[i]->begin(); si != _sampleBufs[i]->end();
            ++si) {
            const Sample *s = *si;
            s->freeReference();
        }
        _sampleBufs[i]->clear();
    }
#endif
    _sampleBufCond.unlock();

#ifndef USE_DEQUE
    delete _sampleBufs[0];
    delete _sampleBufs[1];
#endif
}

size_t SampleBuffer::size() const
{
    n_u::Autolock alock (_sampleBufCond);
#ifdef USE_DEQUE
    return _sampleBuf.size();
#else
    return _inserterBuf->size();
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
        size_t nsamp = _inserterBuf->size();
#endif

	if (_doFinish && nsamp == 0) {
#ifdef DEBUG
            cerr << "SampleSorter calling flush, _source type=" << 
                (_source.getRawSampleSource() ? "raw" : "proc") << endl;
#endif
            _sampleBufCond.unlock();
            // calls finish() on all sample clients.
            flush();
            _sampleBufCond.lock();
	    _finished = true;
	    _doFinish = false;
            continue;
	}

	if (nsamp == 0) {	// no samples, wait
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
        for (;;) {
	    const Sample *s = _sampleBuf.front();
	    _sampleBuf.pop_front();
            _sampleBufCond.unlock();
	    ssum += s->getDataByteLength() + s->getHeaderLength();
	    _source.distribute(s);
#ifdef TEST_CPU_TIME
            if (ntotal++ == 10 * 60 * 60 * 5) {
		cerr << "nloop=" << nloop << " smax=" << smax << " smin=" << smin << " savg=" << (double)savg/nloop << endl;
                exit(1);
            }
#endif
            _sampleBufCond.lock();
            if (_sampleBuf.size() == 0) break;
        }
            
#else
	std::vector<const Sample *>::const_iterator si;
	for (si = _consumerBuf->begin(); si != _consumerBuf->end(); ++si) {
	    const Sample *s = *si;
	    ssum += s->getDataByteLength() + s->getHeaderLength();
	    _source.distribute(s);
#ifdef TEST_CPU_TIME
            if (ntotal++ == 10 * 60 * 60 * 5) {
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
    _sampleBufCond.unlock();
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
void inline SampleBuffer::heapDecrement(size_t bytes)
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
 * flush all samples from buffer, distributing them to SampleClients.
 */
void SampleBuffer::finish() throw()
{
    _sampleBufCond.lock();
    // finish already requested.
    if (_finished || _doFinish) {
        _sampleBufCond.unlock();
        return;
    }

    // After setting this lock, we know that the
    // consumer thread is either:
    //	* waiting on sampleBufCond,
    // 	* distributing samples or,
    //	* hasn't started looping,
    // since those are the only times sampleBufCond is unlocked.

    // If we only did a signal without locking,
    // the setting of _doFinish could be missed before
    // the consumer side waits again.

    _finished = false;
    _doFinish = true;
    _sampleBufCond.unlock();

    // Note that the size() method does a lock of sampleBufCond,
    // so don't call it with a lock held.

    ILOG(("waiting for ") << size() << ' ' <<
        (_source.getRawSampleSource() ? "raw" : "processed") <<
        " samples to drain from SampleBuffer");

#ifdef USE_SAMPLE_SET_COND_SIGNAL
    _sampleBufCond.signal();
#endif

    for (int i = 1; ; i++) {
	struct timespec ns = {0, NSECS_PER_SEC / 10};
	nanosleep(&ns,0);
	if (!(i % 200))
	    ILOG(("waiting for SampleBuffer to empty, size=%d, nwait=%d",
			size(),i));
	_sampleBufCond.lock();
	if (_finished) break;
	_sampleBufCond.unlock();
    }
    _sampleBufCond.unlock();
    ILOG(((_source.getRawSampleSource() ? "raw" : "processed")) <<
        " samples drained from SampleBuffer");
}

bool SampleBuffer::receive(const Sample *s) throw()
{
    size_t slen = s->getDataByteLength() + s->getHeaderLength();

    if (_realTime) {
        dsm_time_t systt = getSystemTime();
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
    _sampleBufCond.unlock();

#ifdef USE_SAMPLE_SET_COND_SIGNAL
    _sampleBufCond.signal();
#endif

    return true;
}

