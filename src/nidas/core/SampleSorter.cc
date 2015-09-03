// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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
/*

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
 */

#include <nidas/core/SampleSorter.h>

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
    _samples(),_sampleSetCond(),_flushCond(),
#ifdef NIDAS_EMBEDDED
    _heapMax(5 * 1000 * 1000),
#else
    _heapMax(50 * 1000 * 1000),
#endif
    _heapSize(0),_heapBlock(false),_heapCond(),_heapExceeded(false),
    _discardedSamples(0),_realTimeFutureSamples(0),_discardWarningCount(1000),
    _doFlush(false),_flushed(true),_dummy(),
    _realTime(false),_maxSorterLengthUsec(0),_lateSampleCacheSize(0)
{
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

    ILOG(("%s: maxSorterLength=%.3f sec, excess=%.3f sec",
	getName().c_str(),(double)_maxSorterLengthUsec/USECS_PER_SEC,
            (double)(_maxSorterLengthUsec-_sorterLengthUsec)/USECS_PER_SEC));
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

    ILOG(("%s: sorterLength=%.3f sec, heapMax=%d, heapBlock=%d",
	getName().c_str(), (double)_sorterLengthUsec/USECS_PER_SEC,
        _heapMax,_heapBlock));


#ifdef DEBUG
    dsm_time_t tlast;
#endif

    _sampleSetCond.lock();

    for (;;) {

	if (isInterrupted()) break;

        size_t nsamp = _samples.size();

        if (nsamp <= _lateSampleCacheSize) {
            if (nsamp == 0) {
                _flushed = true;
                if (_doFlush) {
                    _flushCond.lock();
                    _flushCond.unlock();
                    _flushCond.broadcast();
                    _doFlush = false;
                }
            }

            if (!_doFlush) {	// not enough samples, wait
                _sampleSetCond.wait();
                continue;
            }
	}

        SortedSampleSet::const_iterator rsb = _samples.begin();
        SortedSampleSet::const_iterator rsi;
        dsm_time_t ttlatest = 0;

        if (_doFlush) rsi = _samples.end();
        else {
            // back up over _lateSampleCacheSize number of latest samples before
            // using a sample time to use for the age off.
            SortedSampleSet::const_reverse_iterator latest = _samples.rbegin();
            SortedSampleSet::const_reverse_iterator late = latest;
            for (unsigned int i = 0; i < _lateSampleCacheSize; i++) late++;

            // age-off samples with timetags before this
            dsm_time_t tt = (*late)->getTimeTag() - _sorterLengthUsec;              
            _dummy.setTimeTag(tt);

            // get iterator pointing at first sample not less than dummy
            rsi = _samples.lower_bound(&_dummy);

            ttlatest = (*latest)->getTimeTag();
        }

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
            _sampleSetCond.wait();
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
	std::vector<const Sample *>::const_iterator si = agedsamples.begin();
	size_t ssum = 0;

        // track the maximum length of the sorting buffer in micro seconds,
        // as the time of latest sample - time of earliest sample
        // Since we allow _lateSampleCacheSize samples with anomalous, late
        // time tags, and the most most recent sample added could have a
        // very early time tag, this number can be greater than the requested
        // _sorterLengthUsec
        if (!_doFlush) _maxSorterLengthUsec =
            std::max(ttlatest - (*si)->getTimeTag(),_maxSorterLengthUsec);

	for ( ; si < agedsamples.end(); ++si) {
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

    // warning if remaining samples
    if (_samples.size() > 0)
        WLOG(("SampleSorter (%s) run method exiting, _samples.size()=%zu",
            (_source.getRawSampleSource() ? "raw" : "processed"),_samples.size()));

    SortedSampleSet::const_iterator si;
    for (si = _samples.begin(); si != _samples.end(); ++si) {
	const Sample *s = *si;
	s->freeReference();
    }
    _samples.clear();
    _flushed = true;
    _sampleSetCond.unlock();

    _flushCond.lock();
    _flushCond.unlock();
    _flushCond.broadcast();

    return RUN_OK;
}

void SampleSorter::interrupt()
{
    _sampleSetCond.lock();

    // After setting this lock, we know that the
    // consumer thread is either:
    //	* waiting on sampleSetCond, typically for more samples,
    // 	* distributing samples,
    //	* hasn't started looping, or
    //	* run method has finished
    // since those are the only times sampleSetCond is unlocked.

    // If we only did a signal without locking,
    // the interrupt could be missed if this interrupt() and
    // signal() happened after the consumer thread checked
    // isInterrupted() but before the wait. To prevent this the
    // consumer thread locks _sampleSetCond before the call of
    // isInterrupted() and keeps it locked until the wait().

    Thread::interrupt();
    _sampleSetCond.unlock();
    _sampleSetCond.signal();

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
 * distribute all buffered samples to SampleClients.
 */
void SampleSorter::flush() throw()
{
    _sampleSetCond.lock();

    // After setting this lock, we know that the
    // consumer thread is either:
    //	* waiting on sampleSetCond, typically for more samples,
    // 	* distributing samples,
    //	* hasn't started looping, or
    //	* run method has finished
    // since those are the only times sampleSetCond is unlocked.

    // If _samples.empty() is true, the sorter may not actually
    // be fully flushed, since the other thread may be sending
    // samples. So we have to use a _flushed logical, rather
    // than simply check _samples.empty().
    if (_flushed) {
        _sampleSetCond.unlock();
        return;
    }

    DLOG(("waiting for ") << _samples.size() << ' ' <<
        (_source.getRawSampleSource() ? "raw" : "processed") <<
        " samples to drain from SampleSorter");
    /*
        */

    _doFlush = true;

    _sampleSetCond.unlock();

    // if the consumer thread is waiting, notify it that we don't 
    // want it to wait anymore, we want it to flush
    _sampleSetCond.signal();

    _flushCond.lock();
    while (!_flushed) _flushCond.wait();
    _flushCond.unlock();

    DLOG(((_source.getRawSampleSource() ? "raw" : "processed")) <<
        " samples drained from SampleSorter");
    
    // may want to call flush on the SampleClients.

}

bool SampleSorter::receive(const Sample *s) throw()
{
    unsigned int slen = s->getDataByteLength() + s->getHeaderLength();

    if (_realTime) {
        dsm_time_t samptt = s->getTimeTag();
        dsm_time_t systt = n_u::getSystemTime();
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
	    _sampleSetCond.signal();
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
    _flushed = false;
    _sampleSetCond.unlock();

    _sampleSetCond.signal();

    return true;
}

