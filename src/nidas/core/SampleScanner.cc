
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

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SampleScanner::SampleScanner():
	BUFSIZE(8192),buffer(new char[BUFSIZE]),
	bufhead(0),buftail(0),samp(0),sampDataToRead(0),
	sampDataPtr(0),usecsPerByte(0)
{
    resetStatistics();
}

SampleScanner::~SampleScanner()
{
    delete [] buffer;
}

void SampleScanner::init()
{
    resetStatistics();
}

dsm_time_t SampleScanner::readSamples(DSMSensor* sensor,
	SampleDater* dater)
	throw (n_u::IOException)
{
    size_t len = BUFSIZE - bufhead;	// length to read
    size_t rlen;			// read result
    dsm_time_t tt = 0;

    rlen = sensor->read(buffer+bufhead,len);
    // cerr << "SampleScanner::readSamples::read=" << rlen << endl;
    addNumBytesToStats(rlen);
    bufhead += rlen;

    // process all data in buffer, pass samples onto clients
    for (;;) {
        if (samp) {
	    rlen = bufhead - buftail;	// bytes available in buffer
	    len = sampDataToRead;	// bytes left to fill sample
	    if (rlen < len) len = rlen;
	    memcpy(sampDataPtr,buffer+buftail,len);
	    buftail += len;
	    sampDataPtr += len;
	    sampDataToRead -= len;
	    if (!sampDataToRead) {		// done with sample
		addSampleToStats(samp->getDataByteLength());
		SampleDater::status_t status = sensor->setSampleTime(dater,samp);
		if (status == SampleDater::OK) {
		    tt = samp->getTimeTag();	// return last time tag read
		    samp->setId(sensor->getId());
		    sensor->distributeRaw(samp);
		}
		else {
		    incrementBadTimeTags();
		    samp->freeReference();
		}
		samp = 0;
		// Finished with sample. Check for more data in buffer
	    }
	    else break;		// done with buffer
	}
	// Read the header of the next sample
        if (bufhead - buftail <
		(signed)(len = SIZEOF_DSM_SAMPLE_HEADER))
		break;

	struct dsm_sample header;	// temporary header to read into
	memcpy(&header,buffer+buftail,len);
	buftail += len;

	len = header.length;
	samp = getSample<char>(len);
	// convert time tag to microseconds since 00:00 GMT
	samp->setTimeTag((dsm_time_t)header.timetag * USECS_PER_MSEC);
	samp->setDataLength(len);

	sampDataPtr = (char*) samp->getVoidDataPtr();
	sampDataToRead = len;

    }

    // shift data down. There shouldn't be much - less than a header's worth.
    register char* bp;
    for (bp = buffer; buftail < bufhead; ) 
    	*bp++ = *(buffer + buftail++);

    bufhead = bp - buffer;
    buftail = 0;
    return tt;
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

MessageSampleScanner::MessageSampleScanner():
	SampleScanner(),messageLength(0),separatorAtEOM(true),
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


MessageStreamScanner::MessageStreamScanner(): MessageSampleScanner(),
	MAX_MESSAGE_STREAM_SAMPLE_SIZE(8192),osamp(0),
	separatorCnt(0),outSampLen(0),outSampDataPtr(0),
	sampleOverflows(0),sampleLengthAlloc(0),
	nullTerminate(false)
{
}

MessageStreamScanner::~MessageStreamScanner()
{
    if (osamp) osamp->freeReference();
}


void MessageStreamScanner::init()
{
    /* if termination character is not CR or NL then set
     * nullTermination to false.
     */
    int sl;
    if (getMessageSeparatorAtEOM() &&
    	(sl = getMessageSeparator().length()) > 0) {
	switch (getMessageSeparator()[sl-1]) {
	case '\r':
	case '\n':
	    setNullTerminate(true);
	    break;
	default:
	    setNullTerminate(false);
	    break;
	}
    }
    else setNullTerminate(false);

    sampleLengthAlloc = getMessageLength() + separatorLen + 1;
    if (!osamp) {
	osamp = getSample<char>(sampleLengthAlloc);
	outSampLen = 0;
	outSampDataPtr = osamp->getDataPtr();
	separatorCnt = 0;
    }
    SampleScanner::init();
}

dsm_time_t MessageStreamScanner::readSamplesSepBOM(DSMSensor* sensor,
	SampleDater* dater)
	throw (n_u::IOException)
{
    dsm_time_t tfirstchar = dater->getDataSystemTime();

    dsm_time_t ttres = 0;

    size_t rlen = sensor->read(buffer,BUFSIZE);
    // cerr << "readSamplesSepBOM, rlen=" << rlen << endl;
    const char* eob = buffer + rlen;
    tfirstchar -= rlen * getUsecsPerByte();
    addNumBytesToStats(rlen);

    // empty space needed in sample before processing next character
    int space = separatorLen;
    if (getNullTerminate()) space++;

    for (register const char* cp = buffer; cp < eob; cp++) {
        register char c = *cp;

	// check if we need to enlarge sample
	if (outSampLen + space > osamp->getAllocByteLength()) {
	    if (outSampLen + space > MAX_MESSAGE_STREAM_SAMPLE_SIZE) {

		// reached maximum sample size
		sampleOverflows++;

		if (getNullTerminate()) outSampDataPtr[outSampLen-1] = '\0';
		osamp->setDataLength(outSampLen);

		addSampleToStats(outSampLen);

		// assert: if outSampLen is > 0 then bomtt is valid.
		osamp->setTimeTag(bomtt);
		osamp->setId(sensor->getId());
		ttres = osamp->getTimeTag();
		sensor->distributeRaw(osamp);

		osamp = getSample<char>(sampleLengthAlloc);
		outSampLen = 0;
		outSampDataPtr = osamp->getDataPtr();
		separatorCnt = 0;	// start over
	    }
	    else {		// allocate 50% more space in sample
		size_t newlen = std::max(outSampLen + outSampLen / 2,
				outSampLen + space);
		newlen = std::min(newlen,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
		osamp->reallocateData(newlen);
		outSampDataPtr = osamp->getDataPtr();
	    }
	}

	// loop until we've figured out what to do with this character
        for (;;) {
            if (separatorCnt < separatorLen) {
                // This block is entered if we are currently scanning
                // for the message separator at the beginning of the message.
                if (c == separator[separatorCnt]) {
                    // We now have a character match to the record separator.
                    // increment the separator counter.
                    // if matched entire separator string, previous sample
                    // is ready, ship it.

                    // the receipt time of the initial separator character
		    // is the timetag for the sample.
		    // We haven't yet sent out the previous sample
		    // because we're waiting for a complete match
		    // to the separator string, so we have to
		    // save this timetag.
                    if (separatorCnt == 0) bomtt =
			tfirstchar + (cp - buffer) * getUsecsPerByte();

                    if (++separatorCnt == separatorLen) {
			// send previous
                        if (outSampLen > 0) {
			    if (getNullTerminate()) outSampDataPtr[outSampLen++] =
			    	'\0';
			    osamp->setDataLength(outSampLen);
			    addSampleToStats(outSampLen);

			    ttres = osamp->getTimeTag();
			    osamp->setId(sensor->getId());
			    sensor->distributeRaw(osamp);

			    // good sample
			    // readjust sampleLengthAlloc if this
			    // good sample was bigger.
			    if (outSampLen > sampleLengthAlloc)
				sampleLengthAlloc = std::min(
				    outSampLen,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
			    osamp = getSample<char>(sampleLengthAlloc);
			    outSampLen = 0;
			    outSampDataPtr = osamp->getDataPtr();
			}
                        osamp->setTimeTag(bomtt);
                        // copy separator to next sample
                        ::memcpy(outSampDataPtr+outSampLen,
				separator,separatorCnt);
                        outSampLen += separatorCnt;
                        // leave separatorCnt equal to separatorLen
                    }
                    break;              // character was input
                }
	        else {
                    // At this point:
                    // 1. we're expecting the BOM separator, but
                    // 2. the current character fails a match with the
                    //    BOM string
                    // We'll send the faulty data along anyway so that
                    // the user can see what is going on.

                    if (separatorCnt > 0) {     // previous partial match
                        // We have a partial match to separator,
			// copy chars to the sample data.
                        ::memcpy(outSampDataPtr+outSampLen,
				separator,separatorCnt);
                        outSampLen += separatorCnt;
                        separatorCnt = 0;	// start looking at beg
                        // keep trying matching first character of separator
                        // this won't infinitely loop because now
			// separatorCnt=0
                    }
                    else {              // no match to first character in sep
			if (outSampLen == 0) bomtt =
			    tfirstchar + (cp - buffer) * getUsecsPerByte();
			outSampDataPtr[outSampLen++] = c;
			break;                  // character was input
		    }
		}
	    }
	    else {
		// At this point we have a match to the BOM separator string.
		// and are filling the data buffer.

		// if a variable record length, then check to see
		// if the character matches the initial character
		// in the separator string.
		if (getMessageLength() == 0) {
		    if (c == separator[0]) {    // first char of next
			separatorCnt = 0;
			// loop again to try match to first character
		    }
		    else {
			// no match, treat as data
			outSampDataPtr[outSampLen++] = c;
			break;
		    }
		}
		else {
		    // fixed record length, save character in buffer.
		    // no chance for overflow here, as long as
		    // messageLen < sizeof(buffer)
		    outSampDataPtr[outSampLen++] = c;
		    // If we're done, scan for separator next.
		    if (outSampLen == (unsigned) getMessageLength() +
		    	separatorLen)
			separatorCnt = 0;
		    break;
		}
	    }
	}       // loop until we do something with character
    }           // loop over characters in buffer
    return ttres;
}

dsm_time_t MessageStreamScanner::readSamplesSepEOM(DSMSensor* sensor,
	SampleDater* dater)
	throw (n_u::IOException)
{
    dsm_time_t tfirstchar = dater->getDataSystemTime();

    dsm_time_t ttres = 0;

    size_t rlen = sensor->read(buffer,BUFSIZE);
    // cerr << "readSamplesSepEOM, rlen=" << rlen << endl;
    const char* eob = buffer + rlen;
    addNumBytesToStats(rlen);

    tfirstchar -= rlen * getUsecsPerByte();

    // empty space needed in sample before processing next character
    int space = getNullTerminate() ? 2 : 1;

    for (register const char* cp = buffer; cp < eob; cp++) {
        register char c = *cp;

	// check if we need to enlarge sample
	if (outSampLen + space > osamp->getAllocByteLength()) {
	    // reached maximum sample size
	    if (outSampLen + space > MAX_MESSAGE_STREAM_SAMPLE_SIZE) {
		sampleOverflows++;

		if (getNullTerminate()) outSampDataPtr[outSampLen-1] =
		    '\0';
		osamp->setDataLength(outSampLen);
		addSampleToStats(outSampLen);

		ttres = osamp->getTimeTag();
		osamp->setId(sensor->getId());
		sensor->distributeRaw(osamp);

		osamp = getSample<char>(sampleLengthAlloc);
		outSampLen = 0;
		separatorCnt = 0;
		outSampDataPtr = osamp->getDataPtr();

	    }
	    else {		// allocate 50% more space
		size_t newlen = std::max(outSampLen + outSampLen / 2,
				outSampLen + space);
		newlen = std::min(newlen,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
		osamp->reallocateData(newlen);
		outSampDataPtr = osamp->getDataPtr();
	    }
	}

	if (outSampLen == 0)
	    osamp->setTimeTag(tfirstchar +
		(cp - buffer) * getUsecsPerByte());
	outSampDataPtr[outSampLen++] = c;

	if (getMessageLength() == 0) {
	    // if a variable record length, then check to see
	    // if the character matches the current character
	    // in the end of message separator string.
	    if (c == separator[separatorCnt]) {
		// character matched
		if (++separatorCnt == separatorLen) {
		    // sample is ready
		    if (getNullTerminate())
			outSampDataPtr[outSampLen++] = '\0';
		    osamp->setDataLength(outSampLen);
		    addSampleToStats(outSampLen);

		    ttres = osamp->getTimeTag();
		    osamp->setId(sensor->getId());
		    sensor->distributeRaw(osamp);

		    if (outSampLen > sampleLengthAlloc)
			sampleLengthAlloc = std::min(
			    outSampLen,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
		    osamp = getSample<char>(sampleLengthAlloc);
		    outSampLen = 0;
		    outSampDataPtr = osamp->getDataPtr();
		    separatorCnt = 0;
		}
	    }
	    else {
		// variable record length, no match of current character
		// to EOM string.

		// check for match at beginning of separator string
		// since sepcnt > 0 we won't have a complete match since
		// sepLen then must be > 1.
		if (separatorCnt > 0 &&
		      c == separator[separatorCnt = 0]) separatorCnt++;
	    }
	}
	else {
	    // fixed length record
	    if (outSampLen >= (unsigned) getMessageLength()) {
		// we've read in all our characters, now scan separator string
		// if no match, check from beginning of separator string
#ifdef DEBUG
		cerr << "separatorCnt=" << separatorCnt <<
		    " separatorLen=" << separatorLen << 
		    " c=" << hex << (int)(unsigned char)c << 
		    " sep=" << (int)(unsigned char)separator[separatorCnt] <<
		    dec << endl;
#endif
			
		if (c == separator[separatorCnt] ||
		    c == separator[separatorCnt = 0]) {
		    if (++separatorCnt == separatorLen) {
			if (getNullTerminate())
			    outSampDataPtr[outSampLen++] = '\0';
			osamp->setDataLength(outSampLen);
			addSampleToStats(outSampLen);

			ttres = osamp->getTimeTag();
			osamp->setId(sensor->getId());
			sensor->distributeRaw(osamp);
			if (outSampLen > sampleLengthAlloc)
			    sampleLengthAlloc = std::min(
				outSampLen,MAX_MESSAGE_STREAM_SAMPLE_SIZE);
			osamp = getSample<char>(sampleLengthAlloc);
			outSampLen = 0;
			outSampDataPtr = osamp->getDataPtr();
			separatorCnt = 0;
		    }
		}
	    }
	}
    }
    return ttres;
}

