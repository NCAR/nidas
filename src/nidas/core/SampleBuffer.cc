// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "SampleBuffer.h"

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <vector>
#include <limits> // numeric_limits<>
#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleBuffer::SampleBuffer(const std::string& name,bool raw) :
    SampleThread(name),
    _sampleBufs(),_inserterBuf(),_consumerBuf(),
    _source(raw),_sampleBufCond(),_flushCond(),
    _heapMax(50000000),
    _heapSize(0),_heapBlock(false),_heapCond(),
    _discardedSamples(0),_realTimeFutureSamples(0),_discardWarningCount(1000),
    _doFlush(false),_flushed(true),
    _realTime(false)
{
    _inserterBuf = &_sampleBufs[0];
    _consumerBuf = &_sampleBufs[1];
}

SampleBuffer::~SampleBuffer()
{
    // make sure thread is not running.
    if (isRunning()) interrupt();
    if (!isJoined()) {
        try {
            VLOG(("~SampleBuffer, joining"));
            join();
        }
        catch(const n_u::Exception& e) {
            WLOG(("") << e.what());
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
    return _inserterBuf->size() + _consumerBuf->size();
}

bool SampleBuffer::emptyNoLock() const
{
    return _inserterBuf->empty() && _consumerBuf->empty();
}

/**
 * Thread function.
 */
int SampleBuffer::run()
{
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
    _sampleBufCond.lock();

    for (;;) {


	if (isInterrupted()) break;

        assert(_consumerBuf->empty());
        size_t nsamp = _inserterBuf->size();

	if (nsamp == 0) {	// no samples, wait
            _flushed = true;
            if (_doFlush ) {
                _flushCond.lock();
                _flushCond.unlock();
                _flushCond.broadcast();
                _doFlush = false;
            }
            _sampleBufCond.wait();
            continue;
	}

#ifdef TEST_CPU_TIME
	smax = std::max(smax,nsamp);
	smin = std::min(smin,nsamp);
	savg += nsamp;
        nloop++;
#endif

        // switch buffers
        std::vector<const Sample*>* tmpPtr = _consumerBuf;
        _consumerBuf = _inserterBuf;
        _inserterBuf = tmpPtr;
        _sampleBufCond.unlock();

	// loop over the buffered samples
	size_t ssum = 0;
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
	heapDecrement(ssum);

	_sampleBufCond.lock();
    }

    assert(_consumerBuf->empty());

    // warn if remaining samples
    if (!emptyNoLock())
        WLOG(("SampleBuffer (%s) run method exiting, _samples.size()=%zu",
            (_source.getRawSampleSource() ? "raw" : "processed"),sizeNoLock()));


    for (int i = 0; i < 2; i++) {
        vector<const Sample*>::const_iterator si;
        for (si = _sampleBufs[i].begin(); si != _sampleBufs[i].end();
            ++si) {
            const Sample *s = *si;
            s->freeReference();
        }
        _sampleBufs[i].clear();
    }
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
    _sampleBufCond.signal();
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
