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
    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s: sorterLengthUsec=%d, heapMax=%d, heapBlock=%d",
	getName().c_str(),_sorterLengthUsec,_heapMax,_heapBlock);

    _sampleSetCond.lock();
#ifdef DEBUG
    dsm_time_t tlast;
#endif
    for (;;) {

	if (amInterrupted()) break;

        size_t nsamp = _samples.size();

        // if _doFinish and only one sample, it must be the dummy
        // sample with a into-the-future timetag.
	if (_doFinish && nsamp == 1) {
	    flush();
	    _finished = true;
	    _doFinish = false;
            const Sample *s = *_samples.begin();
            s->freeReference();
            _samples.clear();
            nsamp = 0;
	}

	if (nsamp == 0) {	// no samples, wait
	    _sampleSetCond.wait();
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
	    _sampleSetCond.wait();
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
    // 	* distributing samples or,
    //	* hasn't started looping,
    // since those are the only times sampleSetCond is unlocked.

    // If we only did a signal without locking,
    // the interrupt could be missed.

    Thread::interrupt();
    _sampleSetCond.unlock();
    _sampleSetCond.signal();
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
    // finish already requested.
    if (_finished || _doFinish) return;

    SampleT<char>* eofSample = getSample<char>(0);
    numeric_limits<long long> ll;
    eofSample->setTimeTag(ll.max());
    eofSample->setId(0);

    _sampleSetCond.lock();
    _finished = false;
    _doFinish = true;
    _samples.insert(_samples.end(),eofSample);
    _sampleSetCond.unlock();
    _sampleSetCond.signal();

    for (int i = 1; ; i++) {
	struct timespec ns = {0, NSECS_PER_SEC / 10};
	nanosleep(&ns,0);
	_sampleSetCond.lock();
	if (!(i % 20))
	    n_u::Logger::getInstance()->log(LOG_NOTICE,
		"waiting for buffer to empty, size=%d, _finished=%d",
			_samples.size(),_finished);
	if (_finished) break;
	_sampleSetCond.unlock();
    }
    _sampleSetCond.unlock();

    // calls finish() on all sample clients.
    flush();
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
        // no blocking on heap overflow, discard samples.
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
        // this thread should block until heap gets smaller than _heapMax
	_heapSize += slen;
	// if heapMax will be exceeded, then wait until heapSize comes down
	while (_heapSize > _heapMax) {
	    // Wait until we've distributed enough samples
	    // cerr << "waiting on heap condition, heapSize=" << heapSize << endl;
	    _sampleSetCond.signal();
	    _heapCond.wait();
	    // cerr << "received heap signal, heapSize=" << heapSize << endl;
	}
    }
    _heapCond.unlock();

    s->holdReference();
    _sampleSetCond.lock();
    _samples.insert(_samples.end(),s);
    _sampleSetCond.unlock();

    _sampleSetCond.signal();

    return true;
}

