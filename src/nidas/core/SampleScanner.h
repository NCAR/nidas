/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef NIDAS_CORE_SAMPLESCANNER_H
#define NIDAS_CORE_SAMPLESCANNER_H

#include <nidas/core/SampleClock.h>

#include <nidas/core/dsm_sample.h>
#include <nidas/util/IOException.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace core {

class DSMSensor;

/**
 * A scanner of sample data. Provides a readBuffer() method to read
 * data from a DSMSensor into an internal buffer. Then defines
 * a virtual nextSample() method to extract all available samples
 * from that buffer.
 *
 * Currently there are three implementations of SampleScanner, which
 * differ in how they extract samples from the internal buffer:
 *
 * SampleScanner:
 *    Base class implementation can be used when a DSMSensor has no
 *    notion of message separators in its output.
 *    An A/D converter is an example of such a sensor, for which a kernel
 *    module provides pre-formatted dsm_samples to user code.
 *    A basic SampleScanner does not need to scan input for
 *    message separators, and so it throws 
 *    nidas::util::InvalidParameterExceptions in the default implementation
 *    of these methods for setting message separation parameters.
 *    Default implementation of the readSamples() method reads
 *    pre-formatted dsm_samples from the internal buffer.
 *
 * MessageSampleScanner:
 *    Supports the set/get of the message separation parameters.
 *    but does not itself scan the input for messages.
 *    This implementation is used when a kernel module has 
 *    scanned the imput into samples. The MessageSampleScanner
 *    uses the base class SampleScanner::nextSample() method to
 *    extracts pre-formatted dsm_samples from the buffer.
 *
 * MessageStreamScanner:
 *    Provides sets and gets of message separation parameters used in
 *    parsing input messages from a sensor.  Extracts samples from
 *    the buffer, separating the data into samples by recognizing
 *    separators in the message stream.
 *
 */
class SampleScanner
{
public:

    SampleScanner(int bufsize=8192);

    virtual ~SampleScanner();

    /**
     * Initialize the scanner. Must be called by
     * a user of SampleScanner prior to calling
     * readSamples().
     */
    virtual void init();

    /**
     * setMessageSeparator is not implemented in SampleScanner.
     * Throws nidas::util::InvalidParameterException.
     */
    virtual void setMessageSeparator(const std::string& val)
    	throw(nidas::util::InvalidParameterException)
    {
    	throw nidas::util::InvalidParameterException(
		"setMessageSeparator not supported");
    }

    /**
     * Returns an empty string.
     */
    virtual const std::string& getMessageSeparator() const 
    {
        return emptyString;
    }

    /**
     * Returns an empty string.
     */
    virtual const std::string getBackslashedMessageSeparator() const
    {
	return emptyString; 
    }

    /**
     * setMessageSeparatorAtEOM is not implemented in SampleScanner.
     * Throws nidas::util::InvalidParameterException.
     */
    virtual void setMessageSeparatorAtEOM(bool val)
    	throw(nidas::util::InvalidParameterException)
    {
    	throw nidas::util::InvalidParameterException(
		"setMessageSeparatorAtEOM not supported");
    }
    
    virtual bool getMessageSeparatorAtEOM() const
    {
        return false;
    }

    /**
     * setMessageLength is not implemented in SampleScanner.
     * Throws nidas::util::InvalidParameterException.
     */
    virtual void setMessageLength(unsigned int val)
    	throw(nidas::util::InvalidParameterException) 
    {
    	throw nidas::util::InvalidParameterException(
		"setMessageLength not supported");
    }

    /**
     * Returns 0.
     */
    virtual unsigned int getMessageLength() const
    {
        return 0;
    }

    /**
     * Should the SampleScanner append a null character, '\0',
     * to the messages.
     */
    virtual bool getNullTerminate()  const
    {
        return false;
    }

    void setUsecsPerByte(int val)
    {
        usecsPerByte = val;
    }

    int getUsecsPerByte() const
    {
        return usecsPerByte;
    }

    /**
     * Read from the sensor into the internal buffer of this
     * SampleScanner.
     */
    virtual size_t readBuffer(DSMSensor* sensor)
        throw(nidas::util::IOException);

    /**
     * Read from the sensor into the internal buffer of this
     * SampleScanner, providing a timeout in milliseconds.
     * This will throw nidas::util::IOTimeoutException
     * if the read fails due to a timeout.
     */
    virtual size_t readBuffer(DSMSensor* sensor,int msecTimeout)
        throw(nidas::util::IOException);

    virtual void clearBuffer();

    /**
     * Extract the next sample from the buffer. Returns
     * NULL if there are no more samples in the buffer.
     */
    virtual Sample* nextSample(DSMSensor* sensor);

    size_t getBytesInBuffer() const { return bufhead - buftail; }

    virtual void resetStatistics();

    /**
     * Update the sensor sampling statistics: samples/sec,
     * bytes/sec, min/max sample size, that can be accessed via
     * getObservedSamplingRate(), getObservedDataRate() etc.
     * Should be called every periodUsec by a user of this sensor.
     * @param periodUsec Statistics period.
     */
    virtual void calcStatistics(unsigned long periodUsec);

    size_t getMaxSampleLength() const
    	{ return maxSampleLength[reportIndex]; }

    size_t getMinSampleLength() const
    	{ return minSampleLength[reportIndex]; }

    size_t getBadTimeTagCount() const
    {
	return badTimeTags;
    }

    float getObservedSamplingRate() const;

    float getObservedDataRate() const;

    void addNumBytesToStats(size_t val) { nbytes += val; }

    void addSampleToStats(size_t val)
    {
	nsamples++;
        minSampleLength[currentIndex] =
		std::min(minSampleLength[currentIndex],val);
        maxSampleLength[currentIndex] =
		std::max(maxSampleLength[currentIndex],val);
    }

    void incrementBadTimeTags()
    {
        badTimeTags++;
    }

protected:

    /**
     * Buffer size for reading from sensor.
     */
    const int BUFSIZE;

    char* buffer;

    int bufhead;

    int buftail;

    Sample* osamp;

    struct dsm_sample header;

    size_t outSampRead;
 
    size_t outSampToRead;

    char* outSampDataPtr;

private:

    std::string emptyString;

    time_t initialTimeSecs;

    size_t minSampleLength[2];

    size_t maxSampleLength[2];

    int currentIndex;

    int reportIndex;

    size_t nsamples;

    size_t nbytes;

    size_t badTimeTags;

    /**
    * Observed number of samples per second.
    */
    float sampleRateObs;

    float dataRateObs;

    int usecsPerByte;

};

/**
 * A SampleScanner which supports the set/get of message
 * separation parameters. Typically these parameters are
 * sent down by an implementation of DSMSensor to a kernel
 * module.
 */
class MessageSampleScanner: public SampleScanner
{
public:
    
    MessageSampleScanner(int bufsize=8192);

    ~MessageSampleScanner();

    /**
     * The messageSeparator is the string of bytes that a sensor
     * outputs between messages.  The string may contain
     * baskslash sequences.
     * @see * DSMSensor::replaceBackslashSequences()
     */
    void setMessageSeparator(const std::string& val)
    	throw(nidas::util::InvalidParameterException);

    /**
     * Get message separator string. Any backslash sequences will have
     * been replaced by their intended value.
     */
    const std::string& getMessageSeparator() const
    {
        return messageSeparator;
    }

    /**
     * Get message separator with backslash sequences added back.
     */
    const std::string getBackslashedMessageSeparator() const;

    /**
     * Is the message separator at the end of the message (true),
     * or at the beginning (false)?
     * @param val True means the message scanner expects the separator
     *        at the end of the message, and the next
     *        byte read after the separator is the first
     *        byte of the next message. The timetag of
     *        a sample is the receipt time of the first
     *        byte after the separator of the previous message.
     *        False means the scanner expects the separator
     *        at the beginning of the message, in which case
     *        the timetag of a sample is the receipt time of the
     *        first byte of the messageSeparator.
     */
    void setMessageSeparatorAtEOM(bool val)
    	throw(nidas::util::InvalidParameterException)
    {
	separatorAtEOM = val;
    }

    bool getMessageSeparatorAtEOM() const { return separatorAtEOM; }

    /**
     * Set the message length for this sensor, a zero or positive value.
     * @param val The message length in bytes.
     * A value of zero means the message length is completely variable,
     * and that a scanner is should always be looking for a
     * messageSeparator in the data.
     * A positive value means read in messageLength number of bytes
     * and then start looking for the messageSeparator.
     * The messageLength does not include the length of the
     * messageSeparator.
     *
     * Setting a positive message length is only necessary if
     * there is a chance that the message separator could
     * be found in the data portion of the message. This may
     * be the case if the sensor is generating binary data.
     * A positive message length causes the parser to read
     * messageLength number of bytes before again searching
     * for the separator. This also makes the parsing a bit
     * more efficient. If the messageLength
     * is too small it will not necessarily cause messages to be
     * truncated - all the data up to, and including
     * the separator is still read and passed along for
     * processing.  If the value is larger than the actual
     * message length it will cause two or more messages
     * to be concatenated into one.
     */
    void setMessageLength(unsigned int val)
    	throw(nidas::util::InvalidParameterException) 
    {
	messageLength = val;
    }

    unsigned int getMessageLength() const
    {
        return messageLength;
    }

protected:

    std::string messageSeparator;

    int messageLength;

    bool separatorAtEOM;

    /**
     * messageSeparator in a C string.
     */
    char* separator;

    /**
     * Length of messageSeparator.
     */
    int separatorLen;

};

/**
 * A SampleScanner which supports the set/get of message
 * separation parameters and extracts samples from the
 * internal buffer by recognizing separators in the message stream.
 * Timetags are applied to the samples based on the time of receipt of
 * each chunk of data, corrected backwards by the computed
 * tranmission time of each byte in the chunk.
 * @see setUsecsPerByte().
 */
class MessageStreamScanner: public MessageSampleScanner
{
public:
    
    MessageStreamScanner(int bufsize=1024);

    ~MessageStreamScanner();

    void setMessageSeparatorAtEOM(bool val)
    	throw(nidas::util::InvalidParameterException);

    void setMessageSeparator(const std::string& val)
    	throw(nidas::util::InvalidParameterException);

    void setMessageLength(unsigned int val)
    	throw(nidas::util::InvalidParameterException);

    void setNullTerminate(bool val) 
    {
        nullTerminate = val;
    }

    bool getNullTerminate()  const
    {
        return nullTerminate;
    }

    Sample* nextSample(DSMSensor* sensor)
    {
        return (this->*nextSampleFunc)(sensor);
    }

    size_t readBuffer(DSMSensor* sensor)
        throw(nidas::util::IOException);

    size_t readBuffer(DSMSensor* sensor,int msecTimeout)
        throw(nidas::util::IOException);

protected:

    /**
     * Set parameters general to scanning.  Called by the
     * setMessageXXX methods above.
     */
    void setupMessageScanning();

    /**
     * Method to read input and break it into samples
     * where the message separator occurs at the beginning of
     * the message.
     */
    Sample* nextSampleSepBOM(DSMSensor* sensor);

    /**
     * Method to read input and break it into samples
     * where the message separator occurs at the end of
     * the message.
     */
    Sample* nextSampleSepEOM(DSMSensor* sensor);

    /**
     * Method to read input and break it into samples
     * strictly by record length.
     */
    Sample* nextSampleByLength(DSMSensor* sensor);

    /* ptr to setXXX member function for setting an attribute of this
     * class, based on the value of the tag from the IOStream.
     */
    Sample* (MessageStreamScanner::* nextSampleFunc)(DSMSensor*);

    /**
     * Check that there is room to add nc number of characters to
     * the current sample. If there is room return null pointer.
     * If space can be reallocated in the sample without exceeding
     * MAX_MESSAGE_STREAM_SAMPLE_SIZE then do that and return null.
     * Otherwise return pointer to current sample - which would
     * happen if the BOM or EOM separator strings are not being found
     */
    Sample* checkSampleAlloc(int nc);

private:

    dsm_time_t tfirstchar;

    const size_t MAX_MESSAGE_STREAM_SAMPLE_SIZE;

    int separatorCnt;

    dsm_time_t bomtt;

    int sampleOverflows;

    size_t sampleLengthAlloc;

    bool nullTerminate;

};

}}	// namespace nidas namespace core

#endif
