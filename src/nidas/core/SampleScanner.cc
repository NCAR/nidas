
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SampleScanner.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/Logger.h>
#include <nidas/util/util.h>

#include <iomanip>

#include <sys/select.h>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SampleScanner::SampleScanner(int bufsize):
	BUFSIZE(bufsize),_buffer(new char[BUFSIZE]),
	_bufhead(0),_buftail(0),_osamp(0),_outSampRead(0),
        _outSampToRead(SIZEOF_DSM_SAMPLE_HEADER),
        _outSampDataPtr((char*)&_header),
        _messageLength(0),_separatorAtEOM(true),
	_separator(0),_separatorLen(0),
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

size_t SampleScanner::readBuffer(DSMSensor* sensor)
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
    if (len == 0) return len;
    size_t rlen = sensor->read(_buffer+_bufhead,len);
    // cerr << "SampleScanner::readBuffer, len=" << len << " rlen=" << rlen << endl;
    addNumBytesToStats(rlen);
    _bufhead += rlen;
    return rlen;
}

size_t SampleScanner::readBuffer(DSMSensor* sensor,int msecTimeout)
	throw (n_u::IOException)
{
    if (msecTimeout > 0) {
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(sensor->getReadFd(),&fdset);
        struct timeval to =
            { msecTimeout/MSECS_PER_SEC,
                (msecTimeout % MSECS_PER_SEC) * USECS_PER_MSEC};
        int res;
        if ((res = ::select(sensor->getReadFd()+1,&fdset,0,0,&to)) < 0)
            throw n_u::IOException(sensor->getName(),"read",errno);
        else if (res == 0)
            throw n_u::IOTimeoutException(sensor->getName(),"read");
    }
    return SampleScanner::readBuffer(sensor);
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

/**
 * The messageSeparator is the string of bytes that a sensor
 * outputs between messages.  The string may contain
 * baskslash sequences.
 * @see * nidas::util::replaceBackslashSequences()
 */
void SampleScanner::setMessageSeparatorProtected(const std::string& val)
    throw(n_u::InvalidParameterException)
{
    _messageSeparator = n_u::replaceBackslashSequences(val);

    _separatorLen = _messageSeparator.length();
    delete [] _separator;
    _separator = new char[_separatorLen];
    memcpy(_separator,_messageSeparator.c_str(),_separatorLen);

#ifdef DEBUG
    cerr << "separator val=" << val << endl;
    cerr << "separator (len=" << _separatorLen << "): ";
    for (int i = 0; i < _separatorLen; i++)
        cerr << hex << setw(2) << (int) (unsigned char) _separator[i] << ' ';
    cerr << endl;
#endif
}

DriverSampleScanner::DriverSampleScanner(int bufsize):
	SampleScanner(bufsize)
{
}

Sample* DriverSampleScanner::nextSample(DSMSensor* sensor)
{
    size_t avail = _bufhead - _buftail;	// bytes available in buffer
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

MessageStreamScanner::MessageStreamScanner(int bufsize):
    SampleScanner(bufsize),
    _nextSampleFunc(&MessageStreamScanner::nextSampleByLength),
    MAX_MESSAGE_STREAM_SAMPLE_SIZE(8192),
    _separatorCnt(0),_sampleOverflows(0),_sampleLengthAlloc(0),
    _nullTerminate(false)
{
}

const string MessageStreamScanner::getBackslashedMessageSeparator() const
{
    return n_u::addBackslashSequences(_messageSeparator);
}

void MessageStreamScanner::setMessageLength(unsigned int val)
    throw(n_u::InvalidParameterException)
{
    if (val + _separatorLen > MAX_MESSAGE_STREAM_SAMPLE_SIZE) {
        ostringstream ost;
        ost << "message length=" << val << " plus separator length=" <<
            _separatorLen << " exceed maximum value=" <<
            MAX_MESSAGE_STREAM_SAMPLE_SIZE;
        throw n_u::InvalidParameterException(ost.str());

    }
    _messageLength = val;
    setupMessageScanning();
}

void MessageStreamScanner::setMessageSeparator(const std::string& val)
    throw(n_u::InvalidParameterException)
{
    setMessageSeparatorProtected(val);
    setMessageLength(getMessageLength());   // checks max message len allowed
    setupMessageScanning();
}

void MessageStreamScanner::setMessageSeparatorAtEOM(bool val)
    	throw(nidas::util::InvalidParameterException)
{
    _separatorAtEOM = val;
    setupMessageScanning();
}

void MessageStreamScanner::setupMessageScanning()
{
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
    else _nextSampleFunc = &MessageStreamScanner::nextSampleByLength;

    _sampleLengthAlloc = getMessageLength() + _separatorLen + 
        (getNullTerminate() ? 1 : 0);
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
Sample* MessageStreamScanner::checkSampleAlloc(int nc)
{
    Sample* result = 0;
    if (_outSampRead + (unsigned)nc > _osamp->getAllocByteLength()) {
        // reached maximum sample size
        if (_outSampRead + (unsigned)nc > MAX_MESSAGE_STREAM_SAMPLE_SIZE) {
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
            size_t newlen = std::max(_outSampRead + _outSampRead / 2,
                            _outSampRead + nc);
            newlen = std::min(newlen,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
            _osamp->reallocateData(newlen);  // copies previous data
            _outSampDataPtr = (char*) _osamp->getVoidDataPtr();
        }
    }
    return result;
}

size_t MessageStreamScanner::readBuffer(DSMSensor* sensor)
    throw(n_u::IOException)
{
    // grab the current time, since we assign timetags.
    _tfirstchar = nidas::core::getSystemTime();
    size_t rlen = SampleScanner::readBuffer(sensor);
    // cerr << "readBuffer, rlen=" << rlen << endl;
    _tfirstchar -= rlen * getUsecsPerByte();
    return rlen;
}

size_t MessageStreamScanner::readBuffer(DSMSensor* sensor,int msecTimeout)
    throw(n_u::IOException)
{
    // grab the current time, since we assign timetags.
    _tfirstchar = nidas::core::getSystemTime();
    size_t rlen = SampleScanner::readBuffer(sensor,msecTimeout);
    // cerr << "readBuffer, rlen=" << rlen << endl;
    _tfirstchar -= rlen * getUsecsPerByte();
    return rlen;
}
Sample* MessageStreamScanner::nextSampleSepEOM(DSMSensor* sensor)
{
    Sample* result = 0;

    if (!_osamp) {
	_osamp = getSample<char>(_sampleLengthAlloc);
        _osamp->setId(sensor->getId());
	_outSampDataPtr = (char*) _osamp->getVoidDataPtr();
	_outSampRead = 0;
	_separatorCnt = 0;
    }

    if (_outSampRead == 0)
        _osamp->setTimeTag(_tfirstchar + _buftail * getUsecsPerByte());

    // if getMessageLength() > 0 copy multiple characters
    int nc = getMessageLength() - _outSampRead;
    if (nc > 0) {
        nc = std::min(_bufhead-_buftail,nc);
        if (nc > 0) {
            // actually don't need to checkSampleAlloc() here
            // because if getMessageLength() is > 0, then
            // sampleLengthAlloc should be big enough, but
            // we'll do it to be sure.  Could put an
            // assert here instead.
            if ((result = checkSampleAlloc(nc))) return result;
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

        if ((result = checkSampleAlloc(1 + nterm))) return result;

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
                // adjust sampleLengthAlloc if necessary
                if (_outSampRead > _sampleLengthAlloc ||
                    _sampleLengthAlloc > _outSampRead + _outSampRead / 4)
                        _sampleLengthAlloc = std::min(
                            _outSampRead,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
                _osamp = 0;
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
     * 1. first call, no chars scanned, osamp=NULL
     * 2. last call scanned an entire BOM separator and returned
     *    the sample previous to the separator. Therefore we're
     *    currently reading the portion after the separator. If
     *    getMessageLength() > 0, memcpy available characters, up to
     *    the message length, then start scanning for the next BOM.
     * 3. last call returned 0, meaning there was a partial sample
     *      at end of the previous buffer. osamp then contains
     *      a partial sample. We may or may not be done scanning
     *      for the BOM separator.
     */

    if (!_osamp) {
	_osamp = getSample<char>(_sampleLengthAlloc);
        _osamp->setId(sensor->getId());
	_outSampDataPtr = (char*) _osamp->getVoidDataPtr();
	_outSampRead = 0;
	_separatorCnt = 0;
    }

    if (_separatorCnt == _separatorLen) {
        // BOM separator has been scanned
        // If possible copy multiple characters
        // _outSampRead includes the separator
        int nc = getMessageLength() - (_outSampRead - _separatorCnt);
        if (nc > 0) {
            nc = std::min(_bufhead-_buftail,nc);
            if (nc > 0) {
                if ((result = checkSampleAlloc(nc))) return result;
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
    // for the message separator at the beginning of the message.
    for (;_buftail < _bufhead;) {
        register char c = _buffer[_buftail];

        if ((result = checkSampleAlloc(space))) return result;

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

                    // good sample. adjust sampleLengthAlloc if nec.
                    if (_outSampRead > _sampleLengthAlloc ||
                        _sampleLengthAlloc > _outSampRead + _outSampRead / 4)
                            _sampleLengthAlloc = std::min(
                                _outSampRead,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
                    _osamp = getSample<char>(_sampleLengthAlloc);
                    _osamp->setId(sensor->getId());
                    _outSampDataPtr = (char*) _osamp->getVoidDataPtr();
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

            if (_separatorCnt > 0) {     // previous partial match
                // check for repeated sequence
                if (_separatorCnt > 1 && !memcmp(_separator,_separator+1,_separatorCnt-1)) {
                    // initial repeated character
                    ::memcpy(_outSampDataPtr+_outSampRead,_separator,1);
                    _outSampRead++;
                    _separatorCnt--;
                }
                else {
                    int nrep = _separatorCnt / 2;   // length of sequence
                    if (!(_separatorCnt % 2) && !memcmp(_separator,_separator+nrep,nrep)) {
                        // initial repeated sequence
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
                // this won't infinitely loop because we've reduced _separatorCnt
            }
            else {              // no match to first character in sep
                if (_outSampRead == 0)
                    _osamp->setTimeTag(_tfirstchar + _buftail * getUsecsPerByte());
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
	_outSampRead = 0;
	_separatorCnt = 0;
    }

    if (_outSampRead == 0)
        _osamp->setTimeTag(_tfirstchar + _buftail * getUsecsPerByte());

    int nc = getMessageLength() - _outSampRead;
    nc = std::min(_bufhead-_buftail,nc);
    // cerr << "MessageStreamScanner::nextSampleByLength , nc=" << nc << endl;
    if (nc > 0) {
        // actually don't need to checkSampleAlloc() here
        // because if getMessageLength() is > 0, then
        // sampleLengthAlloc should be big enough, but
        // we'll do it to be sure.  Could put an
        // assert here instead.
        if ((result = checkSampleAlloc(nc))) return result;
        ::memcpy(_outSampDataPtr+_outSampRead,_buffer+_buftail,nc);
        _outSampRead += nc;
        _buftail += nc;
    }
    if (_outSampRead == (unsigned)getMessageLength()) {
        addSampleToStats(_outSampRead);
        result = _osamp;
        _osamp = 0;
    }
    return result;
}

DatagramSampleScanner::DatagramSampleScanner(int bufsize):
	SampleScanner(bufsize),_nullTerminate(false)
{
}

size_t DatagramSampleScanner::readBuffer(DSMSensor* sensor)
	throw (n_u::IOException)
{

    _bufhead = 0;
    _buftail = 0;
    _packetLengths.clear();
    _packetTimes.clear();

    for (;;) {
        int len = sensor->getBytesAvailable();
        if (len == 0) break;
        if (len + _bufhead > BUFSIZE) {
            if (len > BUFSIZE) {
                if (_bufhead > 0) break;    // read big fella next time
                n_u::Logger* logger = n_u::Logger::getInstance();
                logger->log(LOG_WARNING,"%s: huge packet received, %d bytes, will be truncated to %d",
                    sensor->getName().c_str(),len,BUFSIZE);
                len = BUFSIZE;
            }
            else break;
        }

        dsm_time_t tpacket = nidas::core::getSystemTime();
        int rlen = sensor->read(_buffer+_bufhead,len);
        addNumBytesToStats(rlen);
        _bufhead += rlen;
        _packetLengths.push_back(rlen);
        _packetTimes.push_back(tpacket);
    }
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
