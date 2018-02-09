// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include "SampleScanner.h"
#include "DSMSensor.h"
#include "Project.h"
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/util.h>
#include <nidas/Config.h>   // HAVE_PPOLL

#include <iomanip>


#ifdef HAVE_PPOLL
#include <poll.h>
#else
#include <sys/select.h>
#endif

#include <signal.h>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SampleScanner::SampleScanner(int bufsize):
	BUFSIZE(bufsize),_buffer(new char[BUFSIZE]),
	_bufhead(0),_buftail(0),_osamp(0),_header(),_outSampRead(0),
        _outSampToRead(SIZEOF_DSM_SAMPLE_HEADER),
        _outSampDataPtr((char*)&_header),
        _messageSeparator(),
        _messageLength(0),_separatorAtEOM(true),
	_separator(0),_separatorLen(0),
        _emptyString(),_initialTimeSecs(time(0)),
        _minSampleLength(),_maxSampleLength(),
        _currentIndex(0),_reportIndex(0),_nsamples(0),_nbytes(0),
        _badTimeTags(0),_sampleRateObs(0.0),_dataRateObs(0.0),
        _usecsPerByte(0)
{
    resetStatistics();
}

SampleScanner::~SampleScanner()
{
    if (_osamp) _osamp->freeReference();
    delete [] _buffer;
    delete [] _separator;
}

void SampleScanner::init()
{
    resetStatistics();
}

size_t SampleScanner::readBuffer(DSMSensor* sensor,bool& exhausted)
	throw (n_u::IOException)
{
    // shift data down. If the user has read all samples in the
    // previous buffer, there shouldn't be anything to move.
    size_t len = _bufhead - _buftail;
    // cerr << "SampleScanner::readBuffer, len=" << len << endl;
    if (len > 0 && _buftail > 0) ::memmove(_buffer,_buffer+_buftail,len);
    _bufhead = len;
    _buftail = 0;

    len = BUFSIZE - _bufhead;	// length to read
    if (len == 0) {
        exhausted = false;
        return len;
    }
    size_t rlen = sensor->read(_buffer+_bufhead,len);
    // cerr << "SampleScanner::readBuffer, len=" << len << " rlen=" << rlen << endl;

// #define TEST_DEBUG
#ifdef TEST_DEBUG
#define DEBUG
    if (Project::getInstance()->getName() == "test" &&
        sensor->getDSMId() == 1 && sensor->getSensorId() == 10) {
        DLOG(("%s: ",sensor->getName().c_str()) << ", head=" << _bufhead << ", len=" << len << ", rlen=" << rlen << ", data=\"" << string(_buffer,rlen+_bufhead) << "\"");
    }
#undef DEBUG
#endif
    
    addNumBytesToStats(rlen);
    _bufhead += rlen;
    exhausted = rlen < len;
    return rlen;
}

size_t SampleScanner::readBuffer(DSMSensor* sensor,bool& exhausted, int msecTimeout)
	throw (n_u::IOException)
{
    if (msecTimeout > 0) {

#ifdef HAVE_PPOLL
        struct pollfd fds;
        fds.fd = sensor->getReadFd();
#ifdef POLLRDHUP
        fds.events = POLLIN | POLLRDHUP;
#else
        fds.events = POLLIN;
#endif
#else
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(sensor->getReadFd(),&fdset);
#endif

        struct timespec to =
            { msecTimeout/MSECS_PER_SEC,
                (msecTimeout % MSECS_PER_SEC) * NSECS_PER_MSEC};

        // If the user blocks SIGUSR1 prior to calling readBuffer,
        // then we can catch it here in the pselect/ppoll.
        sigset_t sigmask;
        pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
        // unblock SIGUSR1 in ppoll/pselect
        sigdelset(&sigmask,SIGUSR1);

        int res;

#ifdef HAVE_PPOLL
        if ((res = ::ppoll(&fds,1,&to,&sigmask)) < 0)
            throw n_u::IOException(sensor->getName(),"readBuffer",errno);

        if (res == 0)
            throw n_u::IOTimeoutException(sensor->getName(),"read");

        if (fds.revents & POLLERR)
            throw n_u::IOException(sensor->getName(),"readBuffer",errno);
#ifdef POLLRDHUP
        if (fds.revents & (POLLHUP | POLLRDHUP))
#else
        if (fds.revents & (POLLHUP)) 
#endif
            NLOG(("%s: POLLHUP",sensor->getName().c_str()));
#else
        if ((res = ::pselect(sensor->getReadFd()+1,&fdset,0,0,&to,&sigmask)) < 0)
            throw n_u::IOException(sensor->getName(),"readBuffer",errno);
        if (res == 0)
            throw n_u::IOTimeoutException(sensor->getName(),"read");
#endif
    }
    return SampleScanner::readBuffer(sensor,exhausted);
}

void SampleScanner::clearBuffer()
{
    _bufhead = _buftail = 0;
}

void SampleScanner::resetStatistics()
{
    _currentIndex = _reportIndex = 0;

    _sampleRateObs = 0.0;
    _dataRateObs = 0.0;
    _maxSampleLength[0] = _maxSampleLength[1] = 0;
    _minSampleLength[0] = _minSampleLength[1] = UINT_MAX;
    _nsamples = 0;
    _nbytes = 0;
    _badTimeTags = 0;
    _initialTimeSecs = time(0);
}

void SampleScanner::calcStatistics(unsigned int periodUsec)
{
    _reportIndex = _currentIndex;
    _currentIndex = (_currentIndex + 1) % 2;
    _maxSampleLength[_currentIndex] = 0;
    _minSampleLength[_currentIndex] = UINT_MAX;

    _sampleRateObs = ((float)_nsamples / periodUsec) * USECS_PER_SEC;

    _dataRateObs = ((float)_nbytes / periodUsec) * USECS_PER_SEC;

    _nsamples = 0;
    _nbytes = 0;
}

float SampleScanner::getObservedSamplingRate() const {
  
    if (_reportIndex == _currentIndex)
	return (float)_nsamples/
	    std::max((time_t)1,(time(0) - _initialTimeSecs));
    else return _sampleRateObs;
}

float SampleScanner::getObservedDataRate() const {
    if (_reportIndex == _currentIndex)
	return (float)_nbytes /
	    std::max((time_t)1,(time(0) - _initialTimeSecs));
    else return _dataRateObs;
}

DriverSampleScanner::DriverSampleScanner(int bufsize):
	SampleScanner(bufsize)
{
}

Sample* DriverSampleScanner::nextSample(DSMSensor* sensor)
{
    unsigned int avail = _bufhead - _buftail;	// bytes available in buffer
    size_t len;
    Sample* result = 0;
    if (!_osamp) {
        // Read the header of the next sample
        len = std::min(_outSampToRead,avail);
        ::memcpy(_outSampDataPtr+_outSampRead,_buffer+_buftail,len);
        _buftail += len;
        _outSampRead += len;
        _outSampToRead -= len;
        avail -= len;

        if (_outSampToRead != 0) return 0;

        _outSampToRead = _header.length;
        _osamp = getSample<char>(_outSampToRead);
        // convert time tag to microseconds since 00:00 GMT
        _osamp->setTimeTag((dsm_time_t)_header.timetag * sensor->getDriverTimeTagUsecs());
        _osamp->setId(sensor->getId());
        _osamp->setDataLength(_outSampToRead);
        _outSampDataPtr = (char*) _osamp->getVoidDataPtr();
        _outSampRead = 0;
    }

    len = std::min(_outSampToRead,avail);
    ::memcpy(_outSampDataPtr + _outSampRead,_buffer+_buftail,len);
    _buftail += len;
    _outSampRead += len;
    _outSampToRead -= len;

    if (_outSampToRead == 0) {		// done with sample
        addSampleToStats(_osamp->getDataByteLength());
        SampleClock::status_t status =
            SampleClock::getInstance()->addSampleDate(_osamp);

        if (status == SampleClock::OK) result = _osamp;
        else {
            incrementBadTimeTags();
            _osamp->freeReference();
        }
        _osamp = 0;
        _outSampDataPtr = (char*) &_header;
        _outSampRead = 0;
        _outSampToRead = SIZEOF_DSM_SAMPLE_HEADER;
    }
    return result;
}

MessageSampleScanner::MessageSampleScanner(int bufsize):
	DriverSampleScanner(bufsize)
{
}

const string MessageSampleScanner::getBackslashedMessageSeparator() const
{
    return n_u::addBackslashSequences(_messageSeparator);
}

void MessageSampleScanner::setMessageParameters(unsigned int len, const std::string& sep,bool eom)
    throw(n_u::InvalidParameterException)
{
    string sepexp = n_u::replaceBackslashSequences(sep);
    int slen = sepexp.length();
    if (slen == 0 && len == 0)
        throw n_u::InvalidParameterException("no message separator and message length equals 0");

    _messageSeparator = sepexp;
    _separatorLen = slen;
    _separatorAtEOM = eom;
}

MessageStreamScanner::MessageStreamScanner(int bufsize):
    SampleScanner(bufsize),
    _nextSampleFunc(&MessageStreamScanner::nextSampleByLength),
    _tfirstchar(LONG_LONG_MIN),
    MAX_MESSAGE_STREAM_SAMPLE_SIZE(8192),
    _separatorCnt(0),_bomtt(LONG_LONG_MIN),
    _sampleOverflows(0),_sampleLengthAlloc(0),
    _nullTerminate(false),_nsmallSamples(0),_outSampLengthAlloc(0)
{
}

const string MessageStreamScanner::getBackslashedMessageSeparator() const
{
    return n_u::addBackslashSequences(_messageSeparator);
}

void MessageStreamScanner::setMessageParameters(unsigned int len, const std::string& sep,bool eom)
    throw(n_u::InvalidParameterException)
{
    string sepexp = n_u::replaceBackslashSequences(sep);
    int slen = sepexp.length();
    if (len + slen > MAX_MESSAGE_STREAM_SAMPLE_SIZE) {
        ostringstream ost;
        ost << "message length=" << len << " plus separator length=" <<
            slen << " exceed maximum value=" <<
            MAX_MESSAGE_STREAM_SAMPLE_SIZE;
        throw n_u::InvalidParameterException(ost.str());
    }
    if (slen == 0 && len == 0)
        throw n_u::InvalidParameterException("no message separator and message length equals 0");

    _messageSeparator = sepexp;
    _separatorLen = slen;
    delete [] _separator;
    _separator = new char[_separatorLen];
    // separator may contain embedded nulls
    memcpy(_separator,_messageSeparator.c_str(),_separatorLen);
    _separatorAtEOM = eom;
    _messageLength = len;

    /* if message termination character is CR or NL then enable
     * nullTermination.
     */
    if (_separatorLen > 0) {
        if (getMessageSeparatorAtEOM()) _nextSampleFunc =
            &MessageStreamScanner::nextSampleSepEOM;
        else _nextSampleFunc = &MessageStreamScanner::nextSampleSepBOM;

        switch (_separator[_separatorLen-1]) {
        case '\r':
        case '\n':
            setNullTerminate(true);
            break;
        default:
            break;
        }
    }
    else
        _nextSampleFunc = &MessageStreamScanner::nextSampleByLength;

    _sampleLengthAlloc = getMessageLength() + _separatorLen + 
        (getNullTerminate() ? 1 : 0);
    if (getMessageLength() == 0) _sampleLengthAlloc = 16;
}

/*
 * Check that there is room to add nc number of characters to
 * the current sample. If there is room return a null pointer.
 * If space can be reallocated in the sample without exceeding
 * MAX_MESSAGE_STREAM_SAMPLE_SIZE then do that and return null.
 *
 * Otherwise return pointer to current sample. Since that
 * sample has overflowed, it probably means that BOM or EOM
 * separator strings are not being found in the sensor input.
 */
Sample* MessageStreamScanner::requestBiggerSample(unsigned int nc)
{
    Sample* result = 0;
    // reached maximum sample size
    if (_outSampRead + nc > MAX_MESSAGE_STREAM_SAMPLE_SIZE) {
        _sampleOverflows++;
        // null terminate, over-writing last character.
        if (getNullTerminate()) _outSampDataPtr[_outSampRead-1] =
            '\0';
        _osamp->setDataLength(_outSampRead);
        addSampleToStats(_outSampRead);
        result = _osamp;
        _osamp = 0;
    }
    else {		// allocate 50% more space
        unsigned int newlen = std::max(_outSampRead + _outSampRead / 2,
                        _outSampRead + nc);
        newlen = std::min(newlen,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
        // Get a new sample, rather than use _osamp->reallocateData()
        // to grow the data portion of the existing sample.
        // If we use _osamp->reallocateData(), the sample may be
        // returned to a different pool than it was gotten from,
        // which should be avoided.
        SampleT<char>* newsamp = getSample<char>(newlen);
        memcpy(newsamp->getVoidDataPtr(),_osamp->getVoidDataPtr(),_outSampRead);
        newsamp->setTimeTag(_osamp->getTimeTag());
        newsamp->setId(_osamp->getId());
        _osamp->freeReference();
        _osamp = newsamp;
        _outSampDataPtr = (char*) _osamp->getVoidDataPtr();
        _outSampLengthAlloc = _osamp->getAllocByteLength();
    }
    return result;
}

size_t MessageStreamScanner::readBuffer(DSMSensor* sensor, bool& exhausted)
    throw(n_u::IOException)
{
    // grab the current time, since we assign timetags.
    _tfirstchar = n_u::getSystemTime();
    size_t rlen = SampleScanner::readBuffer(sensor,exhausted);
    // cerr << "readBuffer, rlen=" << rlen << endl;
    _tfirstchar -= rlen * getUsecsPerByte();
    return rlen;
}

size_t MessageStreamScanner::readBuffer(DSMSensor* sensor, bool& exhausted, int msecTimeout)
    throw(n_u::IOException)
{
    // grab the current time, since we assign timetags.
    _tfirstchar = n_u::getSystemTime();
    size_t rlen = SampleScanner::readBuffer(sensor,exhausted, msecTimeout);
    // cerr << "readBuffer, rlen=" << rlen << endl;
    _tfirstchar -= rlen * getUsecsPerByte();
    return rlen;
}
Sample* MessageStreamScanner::nextSampleSepEOM(DSMSensor* sensor)
{
    Sample* result = 0;

    if (!_osamp) {
        // first call, or just sent out last sample
        // Wait to allocate next sample, and set its timetag when
        // we have characters.
        if (_buftail == _bufhead) return 0;
	_osamp = getSample<char>(_sampleLengthAlloc);
        _osamp->setId(sensor->getId());
        _osamp->setTimeTag(_tfirstchar + _buftail * getUsecsPerByte());
	_outSampDataPtr = (char*) _osamp->getVoidDataPtr();
        _outSampLengthAlloc = _osamp->getAllocByteLength();
	_outSampRead = 0;
	_separatorCnt = 0;
    }

    // if getMessageLength() > 0 copy multiple characters
    int nc = getMessageLength() - _outSampRead;
    if (nc > 0) {
        nc = std::min(_bufhead-_buftail,(unsigned)nc);
        if (nc > 0) {
            // actually don't need to check for allocated space here
            // because if getMessageLength() is > 0, then
            // _outSampLengthAlloc should be big enough, but
            // we'll do it to be sure.  Could put an
            // assert here instead.
            if (_outSampRead + nc > _outSampLengthAlloc &&
                (result = requestBiggerSample(nc))) return result;
            ::memcpy(_outSampDataPtr+_outSampRead,_buffer+_buftail,nc);
            _outSampRead += nc;
            _buftail += nc;
        }
    }

    // extra space needed for null terminator before processing next character
    int nterm = getNullTerminate() ? 1 : 0;

    // now loop through character by character, until we find the 
    // separator string.
    while (_buftail < _bufhead) {

        if (_outSampRead + 1 + nterm > _outSampLengthAlloc &&
                (result = requestBiggerSample(1 + nterm))) return result;

        char c = _buffer[_buftail++];
        _outSampDataPtr[_outSampRead++] = c;

        // if the character matches the current character
        // in the end of message separator string.
        if (c == _separator[_separatorCnt]) {
            // character matched
            if (++_separatorCnt == _separatorLen) {   // sample is ready
                if (getNullTerminate())
                    _outSampDataPtr[_outSampRead++] = '\0';
                _osamp->setDataLength(_outSampRead);
                addSampleToStats(_outSampRead);

                result = _osamp;
                _osamp = 0;

                // adjust size of next sample to request, if it needs changing
                if (_outSampRead > _sampleLengthAlloc) {
                    _sampleLengthAlloc = std::min(_outSampRead + 16,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
                    _nsmallSamples = 0;
                }
                // check for 100 samples in a row less than _sampleLengthAlloc - 64
                else if (_sampleLengthAlloc > 64 && _outSampRead < _sampleLengthAlloc - 64) {
                    if (++_nsmallSamples > 100) {
                        _sampleLengthAlloc -= 64;
                        _nsmallSamples = 0;
                    }
                }
                else _nsmallSamples = 0;
                break;
            }
        }
        else {
            // no match of current character to EOM string.
            // check for match at beginning of separator string
            //
            // Also handle situation where there are repeated character
            // sequences in the separator.  For example: a separator
            // sequence of xxy, and the input is xxxy.  When you get
            // a failure matching the third character, you shouldn't start
            // completely over scanning for xxy starting at the third x,
            // but should scan for xy starting at the third x.
            // This also happens with a separator of xyxyz and an input of xyxyxyz.
            // One can never tell what kind of separator sequence someone might think of...
            if (_separatorCnt > 0) {
                // initial character repeated
                if (_separatorCnt > 1 && !memcmp(_separator,_separator+1,_separatorCnt-1) &&
                    c == _separator[_separatorCnt-1]);  // leave _separatorCnt as is
                else {
                    // possible repeated sequence
                    int nrep = _separatorCnt / 2;   // length of seq
                    if (!(_separatorCnt % 2) && !memcmp(_separator,_separator+nrep,nrep) &&
                        c == _separator[_separatorCnt = nrep]) _separatorCnt++;
                    // start scan over
                    else if (c == _separator[_separatorCnt = 0]) _separatorCnt++;
                }
            }
        }
    }
    return result;
}

Sample* MessageStreamScanner::nextSampleSepBOM(DSMSensor* sensor)
{
    Sample* result = 0;

    /*
     * scanner will be in one of these states:
     * 1. first call, no chars scanned, _osamp==NULL
     * 2. last sample exceeded MAX_MESSAGE_STREAM_SAMPLE_SIZE before
     *    finding the next BOM. That bogus sample was returned, and now
     *    _osamp==NULL.
     * 3. last call successfully matched the BOM separator and returned
     *    the sample previous to the separator.
     *    In this case _osamp != NULL, _separatorCnt == _separatorLen.
     *    Now to read the portion after the separator into _osamp. If
     *    getMessageLength() > 0, memcpy available characters, up to
     *    the message length, then start scanning for the next BOM.
     * 4. last call returned 0, meaning we have consumed some
     *    characters after the last BOM, but haven't found the next BOM.
     *    In this case, _osamp != NULL and _separatorCnt < _separatorLen.
     */

    if (!_osamp) {
        // first call, or last sample exceeded MAX_MESSAGE_STREAM_SAMPLE_SIZE 
        // Wait to allocate next sample, and set its default timetag when
        // we have characters.
        if (_buftail == _bufhead) return 0;
	_osamp = getSample<char>(_sampleLengthAlloc);
        _osamp->setId(sensor->getId());
	_outSampDataPtr = (char*) _osamp->getVoidDataPtr();
        _outSampLengthAlloc = _osamp->getAllocByteLength();
        // set default timetag in case we never find a BOM again.
        _osamp->setTimeTag(_tfirstchar + _buftail * getUsecsPerByte());
	_outSampRead = 0;
	_separatorCnt = 0;
    }

    if (_separatorCnt == _separatorLen) {
        // BOM separator has been scanned
        // Copy up to getMessageLength() number of characters,
        // or whatever is available in the buffer.
        // _outSampRead includes the separator. If the buffer
        // contains less than getMessageLength() number of characters,
        // copy what is available and return, and then on the next
        // call to this method, this section will re-entered.
        int nc = getMessageLength() - (_outSampRead - _separatorCnt);
        if (nc > 0) {
            nc = std::min(_bufhead-_buftail,(unsigned)nc);
            if (nc > 0) {
                if (_outSampRead + nc > _outSampLengthAlloc &&
                    (result = requestBiggerSample(nc))) return result;
                ::memcpy(_outSampDataPtr+_outSampRead,_buffer+_buftail,nc);
                _outSampRead += nc;
                _buftail += nc;
                if (_buftail == _bufhead) return 0;
            }
        }
        // Copied data portion of sample, starting looking for BOM
        // of next sample.
        _separatorCnt = 0;
    }

    // empty space needed in sample before processing next character
    int space = _separatorLen;
    if (getNullTerminate()) space++;

    // At this point we are currently scanning
    // for the message separator at the beginning of the next message.
    for (;_buftail < _bufhead;) {
        register char c = _buffer[_buftail];

        if (_outSampRead + space > _outSampLengthAlloc &&
            (result = requestBiggerSample(space))) return result;

        if (c == _separator[_separatorCnt]) {
            // We now have a character match to the record separator.
            // increment the separator counter.
            // if matched entire separator string, previous sample
            // is ready, ship it.

            // the receipt time of the initial separator character
            // is the timetag for the sample. Save this time
            // in case the entire separator is not in this buffer.
            if (_separatorCnt == 0) _bomtt =
                _tfirstchar + _buftail * getUsecsPerByte();
            _buftail++;      // used character

            if (++_separatorCnt == _separatorLen) {
                // send previous sample
                if (_outSampRead > 0) {
                    if (getNullTerminate()) _outSampDataPtr[_outSampRead++] =
                        '\0';
                    _osamp->setDataLength(_outSampRead);
                    addSampleToStats(_outSampRead);
                    result = _osamp;

                    // adjust size of next sample to request, if it needs changing
                    if (_outSampRead > _sampleLengthAlloc) {
                        _sampleLengthAlloc = std::min(_outSampRead + 16,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
                        _nsmallSamples = 0;
                    }
                    // check for 100 samples in a row less than _sampleLengthAlloc - 64
                    else if (_sampleLengthAlloc > 64 && _outSampRead < _sampleLengthAlloc - 64) {
                        if (++_nsmallSamples > 100) {
                            _sampleLengthAlloc -= 64;
                            _nsmallSamples = 0;
                        }
                    }
                    else _nsmallSamples = 0;

                    _osamp = getSample<char>(_sampleLengthAlloc);
                    _osamp->setId(sensor->getId());
                    _outSampDataPtr = (char*) _osamp->getVoidDataPtr();
                    _outSampLengthAlloc = _osamp->getAllocByteLength();
                }
                _osamp->setTimeTag(_bomtt);
                // copy separator to beginning of next sample
                ::memcpy(_outSampDataPtr,_separator,_separatorCnt);
                _outSampRead = _separatorCnt;
                // leave _separatorCnt equal to _separatorLen
                if (result) return result;
                // If no previous sample then do a recursive call
                // (or a goto to the beginning of this function).
                // It won't be infinitely recursive, even if the sensor
                // was only sending out BOM strings, because
                // _outSampRead is now > 0
                else return nextSampleSepBOM(sensor);
            }
        }
        else {
            // At this point:
            // 1. we're looking for the BOM separator, but
            // 2. the current character fails a match with the
            //    BOM string
            //
            // Perhaps this is a faulty record, in which case
            // we'll put the unexpected data in the sample anyway so that
            // the user can see what is going on.
            // Or it could be simply that the current message length is
            // greater than getMessageLength() and this is good data.
            //
            // _osamp was allocated in one of the following situations.
            //      1. The very first sample is being scanned. _osamp timetag was set
            //          to estimated receipt time of first character of current sample.
            //      2. The previous sample exceeded MAX_MESSAGE_STREAM_SAMPLE_SIZE. That
            //          sample was sent on, and another sample allocated. We haven't found a
            //          BOM for this sample, and _osamp timetag was set to estimated
            //          receipt time of first character in current sample.
            //      3. We've found a BOM separator. _osamp timetag has been set, _outSampRead will be > 0.

            if (_separatorCnt > 0) {     // previous partial match

                // check for repeated sequence in _separator, e.g. the separator 
                // is "xxz" and the data is "xxx...". The third 'x' in the data has failed
                // to match the 'z' in the separator. _separatorCnt will be 2.
                // Copy the first 'x' to the output sample, check again for a separator match
                // looking for the second 'x' in the separator.
                if (_separatorCnt > 1 && !memcmp(_separator,_separator+1,_separatorCnt-1)) {
                    // initial repeated character
                    ::memcpy(_outSampDataPtr+_outSampRead,_separator,1);
                    _outSampRead++;
                    _separatorCnt--;
                }
                else {
                    int nrep = _separatorCnt / 2;   // length of sequence
                    if (!(_separatorCnt % 2) && !memcmp(_separator,_separator+nrep,nrep)) {
                        // initial repeated sequence in _separator: "xyxyz".
                        // _separatorCnt is 4,6, etc, nrep is at least 2.
                        ::memcpy(_outSampDataPtr+_outSampRead,_separator,nrep);
                        _outSampRead += nrep;
                        _separatorCnt -= nrep;
                    }
                    else {
                        // We have a partial match to separator,
                        // copy chars to the sample data, start scanning over
                        ::memcpy(_outSampDataPtr+_outSampRead,_separator,_separatorCnt);
                        _outSampRead += _separatorCnt;
                        _separatorCnt = 0;	// start scanning for BOM again
                    }
                }
                // We have copied at least one character from the separator into the 
                // current sample, so _outSampRead is now > 0.
                //
                // Note that _buftail has *not* been incremented, i.e. a character has
                // not been consumed from the buffer. This won't infinitely loop
                // because we've reduced _separatorCnt.
            }
            else {              // no match to first character in separator
                _outSampDataPtr[_outSampRead++] = c;
                _buftail++;      // used character
            }
        }
    }

    return result;
}

Sample* MessageStreamScanner::nextSampleByLength(DSMSensor* sensor)
{
    Sample* result = 0;
    if (!_osamp) {
	_osamp = getSample<char>(_sampleLengthAlloc);
        _osamp->setId(sensor->getId());
	_outSampDataPtr = (char*) _osamp->getVoidDataPtr();
        _outSampLengthAlloc = _osamp->getAllocByteLength();
	_outSampRead = 0;
	_separatorCnt = 0;
    }

    if (_outSampRead == 0)
        _osamp->setTimeTag(_tfirstchar + _buftail * getUsecsPerByte());

    int nc = getMessageLength() - _outSampRead;
    // cerr << "MessageStreamScanner::nextSampleByLength , nc=" << nc << endl;
    if (nc > 0) {
        nc = std::min(_bufhead-_buftail,(unsigned)nc);
        if (nc > 0) {
            // actually don't need check for allocated space
            // because if getMessageLength() is > 0, then
            // _outSampLengthAlloc should be big enough, but
            // we'll do it to be sure.  Could put an
            // assert here instead.
            if (_outSampRead + nc > _outSampLengthAlloc &&
                (result = requestBiggerSample(nc))) return result;
            ::memcpy(_outSampDataPtr+_outSampRead,_buffer+_buftail,nc);
            _outSampRead += nc;
            _buftail += nc;
        }
    }

    // Note: if getMessageLength() is zero here, this code will
    // be part of an infinite loop, since it will not consume
    // any characters and be called again-and-again on the
    // same buffer. That situation of message length=0 and
    // no separator should have been caught by the setMessageParameters()
    // method earlier.
    if (_outSampRead == getMessageLength()) {
        addSampleToStats(_outSampRead);
        result = _osamp;
        _osamp = 0;
    }
    return result;
}

DatagramSampleScanner::DatagramSampleScanner(int bufsize):
	SampleScanner(bufsize),
        _packetLengths(),_packetTimes(),
        _nullTerminate(false)
{
}

size_t DatagramSampleScanner::readBuffer(DSMSensor* sensor, bool& exhausted)
	throw (n_u::IOException)
{

    bool exhstd = true;

    _bufhead = 0;
    _buftail = 0;
    _packetLengths.clear();
    _packetTimes.clear();

    size_t len = sensor->getBytesAvailable();
    if (len > BUFSIZE) {
        exhstd = false;
        n_u::Logger* logger = n_u::Logger::getInstance();
        logger->log(LOG_WARNING,"%s: huge packet received, %d bytes, will be truncated to %d",
            sensor->getName().c_str(),len,BUFSIZE);
    }

    for (;;) {

        dsm_time_t tpacket = n_u::getSystemTime();

        size_t rlen;
        try {
            rlen = sensor->read(_buffer+_bufhead, BUFSIZE - _bufhead);
        }
        catch (const n_u::EOFException& e) {
            // nidas::util::Socket will return EOFException
            // on a zero-length read. In this case it is
            // likely just a zero-length packet.
            rlen = 0;
        }
        addNumBytesToStats(rlen);
        _bufhead += rlen;
        _packetLengths.push_back(rlen);
        _packetTimes.push_back(tpacket);

        // size of next packet
        len = sensor->getBytesAvailable();

        // if len is 0, then either:
        //  no datagrams remaining to be read,
        //  or the next datagram has a length of 0.
        // If the latter, then the next read will consume it.
        if (len == 0) break;

        // no room in buffer for next packet, read next time
        if (len + _bufhead > BUFSIZE) {
            exhstd = false;
            break;
        }
    }
    exhausted = exhstd;
    return _bufhead;
}

Sample* DatagramSampleScanner::nextSample(DSMSensor* sensor)
{
    if (_packetLengths.empty()) return 0;

    int plen = _packetLengths.front();

    Sample* samp;
    if (getNullTerminate())
        samp = getSample<char>(plen+1);  // pad with null
    else
        samp = getSample<char>(plen);
        
    samp->setTimeTag(_packetTimes.front());
    samp->setId(sensor->getId());
    ::memcpy(samp->getVoidDataPtr(),_buffer+_buftail,plen);

    if (getNullTerminate())
        ((char*)samp->getVoidDataPtr())[plen] = '\0';

    addSampleToStats(samp->getDataByteLength());

    _buftail += plen;

    _packetLengths.pop_front();
    _packetTimes.pop_front();
    return samp;
}
