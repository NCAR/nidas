
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-12-02 12:21:36 -0700 (Fri, 02 Dec 2005) $

    $LastChangedRevision: 3168 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/DSMSensor.cc $
 ********************************************************************

*/

#include <nidas/core/SampleScanner.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/util/IOTimeoutException.h>

#include <sys/select.h>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SampleScanner::SampleScanner(int bufsize):
	BUFSIZE(bufsize),buffer(new char[BUFSIZE]),
	bufhead(0),buftail(0),osamp(0),outSampRead(0),
        outSampToRead(SIZEOF_DSM_SAMPLE_HEADER),
        outSampDataPtr((char*)&header),usecsPerByte(0)
{
    resetStatistics();
}

SampleScanner::~SampleScanner()
{
    if (osamp) osamp->freeReference();
    delete [] buffer;
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
    size_t len = bufhead - buftail;
    if (len > 0 && buftail > 0) ::memmove(buffer,buffer+buftail,len);
    bufhead = len;
    buftail = 0;

    len = BUFSIZE - bufhead;	// length to read
    if (len == 0) return len;
    size_t rlen = sensor->read(buffer+bufhead,len);
    addNumBytesToStats(rlen);
    bufhead += rlen;
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
    return readBuffer(sensor);
}

Sample* SampleScanner::nextSample(DSMSensor* sensor)
{
    size_t avail = bufhead - buftail;	// bytes available in buffer
    size_t len;
    Sample* result = 0;
    if (!osamp) {
        // Read the header of the next sample
        len = std::min(outSampToRead,avail);
        ::memcpy(outSampDataPtr+outSampRead,buffer+buftail,len);
        buftail += len;
        outSampRead += len;
        outSampToRead -= len;
        avail -= len;

        if (outSampToRead != 0) return 0;

        outSampToRead = header.length;
        osamp = getSample<char>(outSampToRead);
        // convert time tag to microseconds since 00:00 GMT
        osamp->setTimeTag((dsm_time_t)header.timetag * USECS_PER_MSEC);
        osamp->setId(sensor->getId());
        osamp->setDataLength(outSampToRead);
        outSampDataPtr = (char*) osamp->getVoidDataPtr();
        outSampRead = 0;
    }
    len = std::min(outSampToRead,avail);
    ::memcpy(outSampDataPtr + outSampRead,buffer+buftail,len);
    buftail += len;
    outSampRead += len;
    outSampToRead -= len;
    if (outSampToRead == 0) {		// done with sample
        addSampleToStats(osamp->getDataByteLength());
        SampleClock::status_t status =
            SampleClock::getInstance()->addSampleDate(osamp);

        if (status == SampleClock::OK) result = osamp;
        else {
            incrementBadTimeTags();
            osamp->freeReference();
        }
        osamp = 0;
        outSampDataPtr = (char*) &header;
        outSampRead = 0;
        outSampToRead = SIZEOF_DSM_SAMPLE_HEADER;
    }
    return result;
}

void SampleScanner::resetStatistics()
{
    currentIndex = reportIndex = 0;

    sampleRateObs = 0.0;
    dataRateObs = 0.0;
    maxSampleLength[0] = maxSampleLength[1] = 0;
    minSampleLength[0] = minSampleLength[1] = ULONG_MAX;
    nsamples = 0;
    nbytes = 0;
    badTimeTags = 0;
    initialTimeSecs = time(0);
}

void SampleScanner::calcStatistics(unsigned long periodUsec)
{
    reportIndex = currentIndex;
    currentIndex = (currentIndex + 1) % 2;
    maxSampleLength[currentIndex] = 0;

    sampleRateObs = ((float)nsamples / periodUsec) * USECS_PER_SEC;

    dataRateObs = ((float)nbytes / periodUsec) * USECS_PER_SEC;

    nsamples = 0;
    nbytes = 0;
}

float SampleScanner::getObservedSamplingRate() const {
  
    if (reportIndex == currentIndex)
	return (float)nsamples/
	    std::max((long)1,(time(0) - initialTimeSecs));
    else return sampleRateObs;
}

float SampleScanner::getObservedDataRate() const {
    if (reportIndex == currentIndex)
	return (float)nbytes /
	    std::max((long)1,(time(0) - initialTimeSecs));
    else return dataRateObs;
}

MessageSampleScanner::MessageSampleScanner(int bufsize):
	SampleScanner(bufsize),messageLength(0),separatorAtEOM(true),
	separator(0),separatorLen(0)
{
}

MessageSampleScanner::~MessageSampleScanner()
{
    delete [] separator;
}


/**
 * The messageSeparator is the string of bytes that a sensor
 * outputs between messages.  The string may contain
 * baskslash sequences.
 * @see * DSMSensor::replaceBackslashSequences()
 */
void MessageSampleScanner::setMessageSeparator(const std::string& val)
    throw(n_u::InvalidParameterException)
{
    messageSeparator = DSMSensor::replaceBackslashSequences(val);

    separatorLen = messageSeparator.length();
    delete [] separator;
    separator = new char[separatorLen+1];
    strcpy(separator,messageSeparator.c_str());

#ifdef DEBUG
    cerr << "separator=" << hex;
    for (int i = 0; i < separatorLen; i++)
        cerr << (int)(unsigned char)separator[i] << ' ';
    cerr << dec << endl;
    cerr << "getMessageLength=" << getMessageLength() << endl;
#endif

}

/**
 * Get message separator with backslash sequences added back.
 */
const std::string MessageSampleScanner::getBackslashedMessageSeparator() const
{
    return DSMSensor::addBackslashSequences(messageSeparator);
}

MessageStreamScanner::MessageStreamScanner(int bufsize):
    MessageSampleScanner(bufsize),
    nextSampleFunc(&MessageStreamScanner::nextSampleByLength),
    MAX_MESSAGE_STREAM_SAMPLE_SIZE(8192),
    separatorCnt(0),sampleOverflows(0),sampleLengthAlloc(0),
    nullTerminate(false)
{
}

MessageStreamScanner::~MessageStreamScanner()
{
}

void MessageStreamScanner::setMessageLength(unsigned int val)
    throw(n_u::InvalidParameterException)
{
    if (val + separatorLen > MAX_MESSAGE_STREAM_SAMPLE_SIZE) {
        ostringstream ost;
        ost << "message length=" << val << " plus separator length=" <<
            separatorLen << " exceed maximum value=" <<
            MAX_MESSAGE_STREAM_SAMPLE_SIZE;
        throw n_u::InvalidParameterException(ost.str());

    }
    MessageSampleScanner::setMessageLength(val);
    setupMessageScanning();
}

void MessageStreamScanner::setMessageSeparator(const std::string& val)
    throw(n_u::InvalidParameterException)
{
    MessageSampleScanner::setMessageSeparator(val);
    setMessageLength(getMessageLength());   // checks max message len allowed
    setupMessageScanning();
}

void MessageStreamScanner::setMessageSeparatorAtEOM(bool val)
    	throw(nidas::util::InvalidParameterException)
{
    MessageSampleScanner::setMessageSeparatorAtEOM(val);
    setupMessageScanning();
}

void MessageStreamScanner::setupMessageScanning()
{
    /* if message termination character is CR or NL then enable
     * nullTermination.
     */
    setNullTerminate(false);
    if (separatorLen > 0) {
        if (getMessageSeparatorAtEOM()) nextSampleFunc =
            &MessageStreamScanner::nextSampleSepEOM;
        else nextSampleFunc = &MessageStreamScanner::nextSampleSepBOM;

        switch (separator[separatorLen-1]) {
        case '\r':
        case '\n':
            setNullTerminate(true);
            break;
        default:
            break;
        }
    }
    else nextSampleFunc = &MessageStreamScanner::nextSampleByLength;

    sampleLengthAlloc = getMessageLength() + separatorLen + 
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
    if (outSampRead + (unsigned)nc > osamp->getAllocByteLength()) {
        // reached maximum sample size
        if (outSampRead + (unsigned)nc > MAX_MESSAGE_STREAM_SAMPLE_SIZE) {
            sampleOverflows++;
            // null terminate, over-writing last character.
            if (getNullTerminate()) outSampDataPtr[outSampRead-1] =
                '\0';
            osamp->setDataLength(outSampRead);
            addSampleToStats(outSampRead);
            result = osamp;
            osamp = 0;
        }
        else {		// allocate 50% more space
            size_t newlen = std::max(outSampRead + outSampRead / 2,
                            outSampRead + nc);
            newlen = std::min(newlen,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
            osamp->reallocateData(newlen);  // copies previous data
            outSampDataPtr = (char*) osamp->getVoidDataPtr();
        }
    }
    return result;
}

size_t MessageStreamScanner::readBuffer(DSMSensor* sensor)
    throw(n_u::IOException)
{
    // grab the current time, since we assign timetags.
    tfirstchar = nidas::core::getSystemTime();
    size_t rlen = SampleScanner::readBuffer(sensor);
    // cerr << "readBuffer, rlen=" << rlen << endl;
    tfirstchar -= rlen * getUsecsPerByte();
    return rlen;
}

size_t MessageStreamScanner::readBuffer(DSMSensor* sensor,int msecTimeout)
    throw(n_u::IOException)
{
    // grab the current time, since we assign timetags.
    tfirstchar = nidas::core::getSystemTime();
    size_t rlen = SampleScanner::readBuffer(sensor,msecTimeout);
    // cerr << "readBuffer, rlen=" << rlen << endl;
    tfirstchar -= rlen * getUsecsPerByte();
    return rlen;
}
Sample* MessageStreamScanner::nextSampleSepEOM(DSMSensor* sensor)
{
    Sample* result = 0;

    if (!osamp) {
	osamp = getSample<char>(sampleLengthAlloc);
        osamp->setId(sensor->getId());
	outSampDataPtr = (char*) osamp->getVoidDataPtr();
	outSampRead = 0;
	separatorCnt = 0;
    }

    if (outSampRead == 0)
        osamp->setTimeTag(tfirstchar + buftail * getUsecsPerByte());

    // if getMessageLength() > 0 copy multiple characters
    int nc = getMessageLength() - outSampRead;
    if (nc > 0) {
        nc = std::min(bufhead-buftail,nc);
        if (nc > 0) {
            // actually don't need to checkSampleAlloc() here
            // because if getMessageLength() is > 0, then
            // sampleLengthAlloc should be big enough, but
            // we'll do it to be sure.  Could put an
            // assert here instead.
            if ((result = checkSampleAlloc(nc))) return result;
            ::memcpy(outSampDataPtr,buffer+buftail,nc);
            outSampRead += nc;
            buftail += nc;
        }
    }

    // extra space needed for null terminator before processing next character
    int nterm = getNullTerminate() ? 1 : 0;

    // now loop through character by character, until we find the 
    // separator string.
    while (buftail < bufhead) {

        if ((result = checkSampleAlloc(1 + nterm))) return result;

        char c = buffer[buftail++];
        outSampDataPtr[outSampRead++] = c;

        // if the character matches the current character
        // in the end of message separator string.
        if (c == separator[separatorCnt]) {
            // character matched
            if (++separatorCnt == separatorLen) {   // sample is ready
                if (getNullTerminate())
                    outSampDataPtr[outSampRead++] = '\0';
                osamp->setDataLength(outSampRead);
                addSampleToStats(outSampRead);

                result = osamp;
                // adjust sampleLengthAlloc if necessary
                if (outSampRead > sampleLengthAlloc ||
                    sampleLengthAlloc > outSampRead + outSampRead / 4)
                        sampleLengthAlloc = std::min(
                            outSampRead,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
                osamp = 0;
                break;
            }
        }
        else {
            // no match of current character to EOM string.
            // check for match at beginning of separator string
            if (separatorCnt > 0 &&
                  c == separator[separatorCnt = 0]) separatorCnt++;
        }
    }
    return result;
}

Sample* MessageStreamScanner::nextSampleSepBOM(DSMSensor* sensor)
{
    Sample* result = 0;

    /*
     * 1. first call, no chars scanned
     * 2. last call scanned entire BOM separator, returned prev sample
     *      try to read a chunk if possible
     * 3. last call returned 0, partial sample scanned at end of buffer
     *      a. not done with BOM separator
     *      b. done with prev BOM, but haven't started next BOM
     */

    if (!osamp) {
	osamp = getSample<char>(sampleLengthAlloc);
        osamp->setId(sensor->getId());
	outSampDataPtr = (char*) osamp->getVoidDataPtr();
	outSampRead = 0;
	separatorCnt = 0;
    }

    if (separatorCnt == separatorLen) {
        // If possible copy multiple characters
        // outSampRead includes the separator
        int nc = getMessageLength() - (outSampRead - separatorCnt);
        if (nc > 0) {
            nc = std::min(bufhead-buftail,nc);
            if (nc > 0) {
                if ((result = checkSampleAlloc(nc))) return result;
                ::memcpy(outSampDataPtr+outSampRead,buffer+buftail,nc);
                outSampRead += nc;
                buftail += nc;
                if (buftail ==  bufhead) return 0;
            }
        }
        separatorCnt = 0;
    }

    // empty space needed in sample before processing next character
    int space = separatorLen;
    if (getNullTerminate()) space++;

    // At this point we are currently scanning
    // for the message separator at the beginning of the message.
    for (;buftail < bufhead;) {
        register char c = buffer[buftail];

        if ((result = checkSampleAlloc(space))) return result;

        if (c == separator[separatorCnt]) {
            // We now have a character match to the record separator.
            // increment the separator counter.
            // if matched entire separator string, previous sample
            // is ready, ship it.

            // the receipt time of the initial separator character
            // is the timetag for the sample. Save this time
            // in case the entire separator is not in this buffer.
            if (separatorCnt == 0) bomtt =
                tfirstchar + buftail * getUsecsPerByte();
            buftail++;      // used character

            if (++separatorCnt == separatorLen) {
                // send previous sample
                if (outSampRead > 0) {
                    if (getNullTerminate()) outSampDataPtr[outSampRead++] =
                        '\0';
                    osamp->setDataLength(outSampRead);
                    addSampleToStats(outSampRead);
                    result = osamp;

                    // good sample. adjust sampleLengthAlloc if nec.
                    if (outSampRead > sampleLengthAlloc ||
                        sampleLengthAlloc > outSampRead + outSampRead / 4)
                            sampleLengthAlloc = std::min(
                                outSampRead,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
                    osamp = getSample<char>(sampleLengthAlloc);
                    osamp->setId(sensor->getId());
                    outSampDataPtr = (char*) osamp->getVoidDataPtr();
                }
                osamp->setTimeTag(bomtt);
                // copy separator to beginning of next sample
                ::memcpy(outSampDataPtr,separator,separatorCnt);
                outSampRead = separatorCnt;
                // leave separatorCnt equal to separatorLen
                if (result) return result;
            }
        }
        else {
            // At this point:
            // 1. we're looking for the BOM separator, but
            // 2. the current character fails a match with the
            //    BOM string
            // We'll put the faulty data in the sample anyway so that
            // the user can see what is going on.

            if (separatorCnt > 0) {     // previous partial match
                // We have a partial match to separator,
                // copy chars to the sample data.
                ::memcpy(outSampDataPtr+outSampRead,separator,separatorCnt);
                outSampRead += separatorCnt;
                separatorCnt = 0;	// start looking at beg
                // keep trying matching first character of separator
                // this won't infinitely loop because now separatorCnt=0
            }
            else {              // no match to first character in sep
                if (outSampRead == 0)
                    osamp->setTimeTag(tfirstchar + buftail * getUsecsPerByte());
                outSampDataPtr[outSampRead++] = c;
                buftail++;      // used character
            }
        }
    }

    return result;
}

Sample* MessageStreamScanner::nextSampleByLength(DSMSensor* sensor)
{
    Sample* result = 0;
    if (!osamp) {
	osamp = getSample<char>(sampleLengthAlloc);
        osamp->setId(sensor->getId());
	outSampDataPtr = (char*) osamp->getVoidDataPtr();
	outSampRead = 0;
	separatorCnt = 0;
    }

    if (outSampRead == 0)
        osamp->setTimeTag(tfirstchar + buftail * getUsecsPerByte());

    int nc = getMessageLength() - outSampRead;
    nc = std::min(bufhead-buftail,nc);
    if (nc > 0) {
        // actually don't need to checkSampleAlloc() here
        // because if getMessageLength() is > 0, then
        // sampleLengthAlloc should be big enough, but
        // we'll do it to be sure.  Could put an
        // assert here instead.
        if ((result = checkSampleAlloc(nc))) return result;
        ::memcpy(outSampDataPtr+outSampRead,buffer+buftail,nc);
        outSampRead += nc;
        buftail += nc;
    }
    if (outSampRead == (unsigned)getMessageLength()) {
        result = osamp;
        osamp = 0;
    }
    return result;
}
