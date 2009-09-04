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

    _sampleSetCond.lock();
    list<const Sample*> tmplist = _samples;
    _samples.clear();
    _sampleSetCond.unlock();

#ifdef DEBUG
    cerr << "freeing reference on samples" << endl;
#endif
    list<const Sample*>::const_iterator si;
    for (si = tmplist.begin(); si != tmplist.end(); ++si) {
	const Sample *s = *si;
	s->freeReference();
    }
}

/**
 * Thread function.
 */
int SampleBuffer::run() throw(n_u::Exception)
{
    _sampleSetCond.lock();
#ifdef DEBUG
    dsm_time_t tlast;
#endif
    for (;;) {

	if (amInterrupted()) break;

        size_t nsamp = _samples.size();

	if (_doFinish && nsamp == 0) {
            // calls finish() on all sample clients.
#ifdef DEBUG
            cerr << "SampleSorter calling flush, _source type=" << 
                (_source.getRawSampleSource() ? "raw" : "proc") << endl;
#endif
            _sampleSetCond.unlock();
            flush();
            _sampleSetCond.lock();
	    _finished = true;
	    _doFinish = false;
            continue;
	}

	if (nsamp == 0) {	// no samples, wait
	    _sampleSetCond.wait();
	    continue;
	}

        list<const Sample*> tmplist = _samples;
        _samples.clear();
        _sampleSetCond.unlock();

	// loop over the buffered samples
	std::list<const Sample *>::const_iterator si;
	size_t ssum = 0;
	for (si = tmplist.begin(); si != tmplist.end(); ++si) {
	    const Sample *s = *si;
	    ssum += s->getDataByteLength() + s->getHeaderLength();
#ifdef DEBUG
            cerr << "distributing sample, id=" <<
                s->getDSMId() << ',' << s->getSpSId() << endl;
#endif
	    _source.distribute(s);
	}
	heapDecrement(ssum);

	_sampleSetCond.lock();
    }
    _sampleSetCond.unlock();
    return RUN_OK;
}

void SampleBuffer::interrupt()
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
    _sampleSetCond.lock();
    // finish already requested.
    if (_finished || _doFinish) {
        _sampleSetCond.unlock();
        return;
    }

    // After setting this lock, we know that the
    // consumer thread is either:
    //	* waiting on sampleSetCond,
    // 	* distributing samples or,
    //	* hasn't started looping,
    // since those are the only times sampleSetCond is unlocked.

    // If we only did a signal without locking,
    // the interrupt could be missed.

    _finished = false;
    _doFinish = true;
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
}

bool SampleBuffer::receive(const Sample *s) throw()
{
    size_t slen = s->getDataByteLength() + s->getHeaderLength();

    if (_realTime) {
        dsm_time_t systt = getSystemTime();
        dsm_time_t samptt = s->getTimeTag();
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
	    // cerr << "waiting on heap condition, heapSize=" << _heapSize <<
              //   " heapMax=" << _heapMax << endl;
	    _sampleSetCond.signal();
	    _heapCond.wait();
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

