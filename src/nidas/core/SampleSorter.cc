// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 8; -*-
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

#include "SampleSorter.h"

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <vector>
#include <iostream>

#include "SampleTracer.h"

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;
using nidas::util::endlog;
using nidas::util::LogScheme;
using nidas::util::Synchronized;


SampleSorter::SampleSorter(const std::string& name,bool raw) :
    SampleThread(name),_source(raw),
    _sorterLengthUsec(0),
    _sorting(false),
    _samples(),_fifoSamples(),_sampleSetCond(),_flushCond(),
    _heapMax(50 * 1000 * 1000),
    _heapSize(0),_heapBlock(false),_heapCond(),_heapExceeded(false),
    _discardedSamples(0),_realTimeFutureSamples(0),_earlySamples(0),
    _discardWarningCount(1000), _earlyWarningCount(_discardWarningCount),
    _doFlush(false),_flushed(true),_dummy(),
    _realTime(false),_maxSorterLengthUsec(0),_lateSampleCacheSize(0)
{
    // make sure _sorting and _sorterLengthUsec are kept consistent.
    setLengthSecs(0.250);
    // Allow the discard warning count to be overridden.
    _discardWarningCount =
        LogScheme::current().getParameterT("sample_sorter_discard_warning_count",
                                           _discardWarningCount);
    _earlyWarningCount =
        LogScheme::current().getParameterT("sample_sorter_early_warning_count",
                                           _earlyWarningCount);
}


bool SampleSorter::isSorting() const
{
    return _sorting;
}


bool SampleSorter::isSynchronized() const
{
    return !_realTime && !_sorting;
}


SampleSorter::~SampleSorter()
{
    // make sure thread is not running.
    if (isRunning()) interrupt();
    if (!isJoined()) {
        try {
            VLOG(("~SampleSorter, joining"));
            join();
        }
        catch(const n_u::Exception& e) {
            WLOG(("%s",e.what()));
        }
    }

    // It is possible for another thread to pass samples to receive() even
    // though the sorter was interrupted, so just make sure they've been
    // released.
    size_t nsamples = size();
    if (nsamples)
    {
        DLOG(("SampleSorter: releasing ") << nsamples << " samples "
             << "received after sorter stopped.");
        std::vector<const Sample*> agedsamples;
        getNextSamples(agedsamples, true);
        for (auto s: agedsamples) {
            s->freeReference();
        }
    }
    ILOG(("%s: maxSorterLength=%.3f sec, excess=%.3f sec, discarded=%d",
          getName().c_str(),
          (double)_maxSorterLengthUsec/USECS_PER_SEC,
          (double)(_maxSorterLengthUsec-_sorterLengthUsec)/USECS_PER_SEC,
          _discardedSamples));
}


void
SampleSorter::
getNextSortedSamples(std::vector<const Sample*>& agedsamples, bool flush)
{
    // the _sampleSetCond lock is already held
    SortedSampleSet::const_iterator rsb = _samples.begin();
    SortedSampleSet::const_iterator rsi;
    SortedSampleSet::const_reverse_iterator latest = _samples.rbegin();
    SortedSampleSet::const_reverse_iterator late = latest;
    dsm_time_t ttlatest = 0;

    if (flush) {
        rsi = _samples.end();
    }
    else {
        // back up over _lateSampleCacheSize number of latest samples before
        // using a sample time to use for the age off.
        for (unsigned int i = 0;
             i < _lateSampleCacheSize && late != _samples.rend(); i++)
        {
            late++;
        }
        if (late == _samples.rend()) {
            return;
        }

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
        // either way return here and return no samples
        return;
    }
    else {
        // grab the samples before the iterator
        agedsamples.insert(agedsamples.end(), rsb, rsi);
        // remove samples from sorted multiset
        _samples.erase(rsb, rsi);
    }

    // track the maximum length of the sorting buffer in micro seconds,
    // as the time of latest sample - time of earliest sample
    // Since we allow _lateSampleCacheSize samples with anomalous, late
    // time tags, and the most most recent sample added could have a
    // very early time tag, this number can be greater than the requested
    // _sorterLengthUsec
    if (!flush && !agedsamples.empty())
    {
        const Sample* s = *agedsamples.begin();
        _maxSorterLengthUsec = std::max(ttlatest - s->getTimeTag(),
                                        _maxSorterLengthUsec);
    }
}


void SampleSorter::getNextSamples(std::vector<const Sample*>& samples,
                                  bool flush)
{
    if (_sorting)
    {
        getNextSortedSamples(samples, flush);
    }
    else
    {
        samples.insert(samples.end(), _fifoSamples.begin(),
                       _fifoSamples.end());
        _fifoSamples.clear();
    }
}


size_t SampleSorter::size() const
{
    size_t nsamp = _sorting ? _samples.size() : _fifoSamples.size();
    return nsamp;
}


void SampleSorter::setLengthSecs(float val)
{
    if (!isRunning())
    {
        _sorterLengthUsec = (unsigned int)((double)val * USECS_PER_SEC);
        _sorting = _sorterLengthUsec > 0;
    }
    else
    {
        NLOG(("") << getName() << ": cannot set sorter length while running");
    }
}


float SampleSorter::getLengthSecs() const
{
    return (double)_sorterLengthUsec / USECS_PER_SEC;
}


/**
 * Thread function.
 */
int SampleSorter::run() throw(n_u::Exception)
{
    ILOG(("%s: realtime=%d, sorterLength=%.3f sec, lateSampleCache=%d, "
          "heapMax=%d, heapBlock=%d",
          getName().c_str(), _realTime,
          (double)_sorterLengthUsec/USECS_PER_SEC,
          _lateSampleCacheSize, _heapMax, _heapBlock));

    // technically there is no need to buffer samples if we are not in
    // real-time mode and not sorting, since we can immediately distribute
    // them, and therefore there is no need for a separate thread to hang
    // around.
    if (isSynchronized())
    {
        return RUN_OK;
    }

    static n_u::LogContext sslog(LOG_VERBOSE, "sample_sorter");
    static n_u::LogMessage ssmsg(&sslog);
    static SampleTracer st;
    // Allow some memory reuse rather than reallocating every loop.
    std::vector<const Sample*> agedsamples;

    _sampleSetCond.lock();

    while (! isInterrupted()) {

        size_t nsamp = size();

        // Mark that all samples have been flushed, and if flushing was
        // requested, then signal that it is done.
        if (nsamp == 0) {
            _flushed = true;
            if (_doFlush) {
                _flushCond.lock();
                _flushCond.broadcast();
                _flushCond.unlock();
                _doFlush = false;
            }
        }

        if (nsamp == 0 || (_sorting && nsamp <= _lateSampleCacheSize))
        {
            // not enough samples, wait if not flushing all samples
            if (!_doFlush) {
                _sampleSetCond.wait();
                continue;
            }
        }

        agedsamples.clear();
        getNextSamples(agedsamples, _doFlush);
        if (agedsamples.empty())
        {
            _sampleSetCond.wait();
            continue;
        }

        // done pulling samples from the buffer, free the lock
        _sampleSetCond.unlock();

        if (sslog.active())
        {
            ssmsg << getName() << ": "
                << agedsamples.size() << " samples";
            if (_doFlush)
                ssmsg << " being flushed";
            else if (_sorting)
                ssmsg << " aged off";
            else
                ssmsg << " popped from fifo";
            ssmsg << ", from "
                << st.format_time((*agedsamples.begin())->getTimeTag())
                << " to "
                << st.format_time((*agedsamples.rbegin())->getTimeTag())
                << endlog;
        }

        // loop over the aged samples.  sum the sample sizes to decrease
        // the heap usage accordingly.
        size_t ssum = 0;
        dsm_time_t tlast = 0;

        for (auto s : agedsamples)
        {
            ssum += s->getDataByteLength() + s->getHeaderLength();
            if (sslog.active())
            {
                dsm_time_t tsamp = s->getTimeTag();
                if (tsamp < tlast) {
                    ssmsg << "tsamp=" << st.format_time(tsamp)
                            << " tlast=" << st.format_time(tlast)
                            << " id=" << GET_DSM_ID(s->getId()) << ','
                            << GET_SHORT_ID(s->getId())
                            << " sorterLength=" << _sorterLengthUsec/USECS_PER_MSEC
                            << " msec" << endlog;
                }
                tlast = tsamp;
            }
            if (st.active(s))
            {
                st.msg(s, "distribute ") << " from " << getName() << endlog;
            }
            _source.distribute(s);
        }
        heapDecrement(ssum);

        _sampleSetCond.lock();
    }

    // warning if remaining samples
    size_t nsamp = size();
    if (nsamp > 0)
        WLOG(("SampleSorter (%s) run method exiting, _samples.size()=%zu",
             (_source.getRawSampleSource() ? "raw" : "processed"), nsamp));

    agedsamples.clear();
    getNextSamples(agedsamples, true);
    for (auto s: agedsamples)
        s->freeReference();
    _flushed = true;
    _sampleSetCond.unlock();

    _flushCond.lock();
    _flushCond.broadcast();
    _flushCond.unlock();

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
    _sampleSetCond.signal();
    _sampleSetCond.unlock();

    // Thread may also be waiting on a flush, tell it to quit anyway.
    _flushCond.lock();
    _flushCond.signal();
    _flushCond.unlock();

    // In case a thread calling receive is waiting on heapCond.
    _heapCond.lock();
    // make sure that it won't block on the heapMax again
    _heapSize = 0;
    _heapCond.signal();
    _heapCond.unlock();
}

// We've removed some samples from the heap. Decrement heapSize
// and signal waiting threads if the heapSize has shrunk enough.
void SampleSorter::heapDecrement(size_t bytes)
{
    _heapCond.lock();
    if (!_heapBlock) {
        _heapSize -= bytes;
    }
    else {
        if (_heapExceeded) {	// receive() method is waiting on _heapCond
            _heapSize -= bytes;
            // To reduce thrashing, wait until heap has decreased to 50% of
            // _heapMax before signalling a waiting thread.
            // Note there is a possibility that more than 50% of the heap
            // is really needed to hold a sorter length's amount of samples.
            // That situation is caught by the run method when there are no
            // aged samples.
            if (_heapSize < _heapMax/2) {
                DLOG(("") << getName() << ": heap(" << _heapSize
                     << ") < 1/2 * max(" << _heapMax << "), resuming");
                _heapCond.signal();
                _heapExceeded = false;
            }
        }
        else {
            _heapSize -= bytes;
        }
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

    // Even if size() is zero, the sorter may not actually be fully flushed,
    // since the other thread may be sending samples. So we have to use a
    // _flushed logical instead.
    if (_flushed) {
        _sampleSetCond.unlock();
        return;
    }

    _doFlush = true;

    // if the consumer thread is waiting, notify it that we don't 
    // want it to wait anymore, we want it to flush
    _sampleSetCond.signal();
    int nsamples = size();
    _sampleSetCond.unlock();

    _flushCond.lock();
    int nloop = 0;
    while (!_flushed && !isInterrupted())
    {
        if (! nloop++)
        {
            DLOG(("waiting for ") << nsamples << ' ' 
                 << (_source.getRawSampleSource() ? "raw" : "processed")
                 << " samples to drain from SampleSorter");
        }
        _flushCond.wait();
    }
    _flushCond.unlock();

    if (!isInterrupted())
    {
        DLOG(((_source.getRawSampleSource() ? "raw" : "processed")) <<
             " samples drained from SampleSorter");
    }
    else
    {
        DLOG(((_source.getRawSampleSource() ? "raw" : "processed")) <<
             " SampleSorter interrupted, samples may not have drained.");
    }

    // may want to call flush on the SampleClients.
}


bool SampleSorter::heapAvailable(unsigned int slen)
{
    // Check if the heapSize will exceed heapMax
    _heapCond.lock();
    if (!_heapBlock) {
        // Real-time behaviour, discard samples rather than blocking threads
        if (_heapSize + slen > _heapMax) {
            _heapExceeded = true;
            _heapCond.unlock();
            if (!(_discardedSamples++ % _discardWarningCount))
                WLOG(("%d discarded samples because "
                    "heapSize(%d) + sampleSize(%d) is > than heapMax(%d)",
                    _discardedSamples, _heapSize, slen, _heapMax));
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
            // while the producer thread is waiting in the receive() method
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
    return true;
}


bool SampleSorter::validTime(const Sample *s)
{
    dsm_time_t samptt = s->getTimeTag();
    dsm_time_t systt = n_u::getSystemTime();
    // On DSMs with samples which are timetagged by an IRIG, the IRIG clock
    // can be off if it doesn't have a lock
    if (samptt > systt + USECS_PER_SEC * 2) {
        if (!(_realTimeFutureSamples++ % _discardWarningCount))
            WLOG(("sample with timetag in future by %f secs. time: ",
                    (float)(samptt - systt) / USECS_PER_SEC)
                    << n_u::UTime(samptt).format(true,"%Y %b %d %H:%M:%S.%3f")
                    << " id=" << GET_DSM_ID(s->getId()) << ','
                    << GET_SPS_ID(s->getId())
                    << " total future samples=" << _realTimeFutureSamples);
        return false;
    }
    return true;
}


void SampleSorter::checkEarlySample(const Sample *s)
{
    // If a sample arrives that is prior to the current sorter time window
    // then it may not be sorted, depending on whether the consumer thread
    // has caught up to this producer thread. We warn about this condition
    // but do not discard samples.
    SortedSampleSet::const_reverse_iterator latest = _samples.rbegin();
    if (latest != _samples.rend() &&
        s->getTimeTag() < (*latest)->getTimeTag() - _sorterLengthUsec)
    {
        if (!(_earlySamples++ % _earlyWarningCount))
        {
            dsm_time_t wend = (*latest)->getTimeTag();
            dsm_time_t wbegin = wend - _sorterLengthUsec;
            WLOG(("Early sample (%d,%d) @ ", 
                  s->getDSMId(), s->getSpSId())
                 << SampleTracer::format_time(s->getTimeTag())
                 << " (" << _earlySamples << " total)"
                 << ": prior to sorter window ["
                 << SampleTracer::format_time(wbegin) << ", "
                 << SampleTracer::format_time(wend) << "]");
        }
    }
}


bool SampleSorter::receive(const Sample *s) throw()
{
    // If not real-time and not sorting, then there is no point in even
    // buffering, and in fact NIDAS applications like data_stats rely on this
    // lock-step single-thread behavior.  This is equivalent to the
    // SampleBuffer behavior, when it was instantiated in place of a
    // SampleSorter when sorting length was zero.
    if (isSynchronized())
    {
        s->holdReference();
        _source.distribute(s);
        return true;
    }
    unsigned int slen = s->getDataByteLength() + s->getHeaderLength();

    // we only care about errant time tags if actually using them to sort,
    // otherwise it's something else's problem.
    if (_realTime && _sorting) {
        if (!validTime(s))
            return false;
    }

    if (!heapAvailable(slen))
    {
        return false;
    }

    // sample is being added to the buffer.
    Synchronized lock(_sampleSetCond);
    if (_sorting)
    {
        checkEarlySample(s);
    }

    s->holdReference();
    if (_sorting)
        _samples.insert(_samples.end(), s);
    else
        _fifoSamples.push_back(s);
    _flushed = false;
    _sampleSetCond.signal();
    return true;
}
