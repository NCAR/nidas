/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-12-05 19:38:10 -0700 (Mon, 05 Dec 2005) $

    $LastChangedRevision: 3187 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/DSMSensor.h $
 ********************************************************************

*/
#ifndef NIDAS_CORE_SAMPLESCANNER_H
#define NIDAS_CORE_SAMPLESCANNER_H

#include <nidas/core/SampleDater.h>

#include <nidas/core/dsm_sample.h>
#include <nidas/util/IOException.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace core {

class DSMSensor;

/**
 * A scanner of sample data. Reads samples from an input
 * and distributes the samples to SampleClients of a DSMSensor.
 *
 * Currently there are three implementations of SampleScanner.
 * MessageStreamScanner:
 *    Provides sets and gets of message separation parameters used in
 *    parsing input messages from a sensor.
 *    Reads the sensor output from an IO device in user space,
 *    separating messages into samples by recognizing separators
 *    in the message stream.
 * MessageSampleScanner:
 *    Supports the set/get of the message separation parameters
 *    like MessageStreamScanner, but does not itself scan the input
 *    for messages.  The message scanning is done in a kernel
 *    module, and the MessageSampleScanner reads pre-formatted
 *    dsm_samples from an IO device supported by the kernel module.
 * SampleScanner:
 *    Sensor has no notion of message separators.  An A/D converter
 *    is an example of such a sensor. A kernel module provides
 *    pre-formatted dsm_samples over a IO device to the
 *    OtherSampleScanner.
 */

/**
 * A basic scanner of pre-formatted dsm_samples from a sensor.
 * A basic SampleScanner does not need to scan input for
 * message separators, and so it throws 
 * nidas::util::InvalidParameterExceptions in the methods for setting
 * message separation parameters.  readSamples() method reads 
 * pre-formatted dsm_samples from an IO device.
 */
class SampleScanner
{
public:

    SampleScanner();

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
    virtual void setMessageLength(int val)
    	throw(nidas::util::InvalidParameterException) 
    {
    	throw nidas::util::InvalidParameterException(
		"setMessageLength not supported");
    }

    /**
     * Returns 0.
     */
    virtual int getMessageLength() const
    {
        return 0;
    }

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
     * Read input from an IODevice, converting it to Samples,
     * assigning full time tags to the Samples which include
     * the date, and distribute them to the SampleClients of
     * a DSMSensor.
     *
     * SampleScanner provides an implemention of readSamples
     * for reading pre-formatted dsm_samples from a DSMSensor.
     */
    virtual dsm_time_t readSamples(DSMSensor* sensor,SampleDater* dater)
    	throw(nidas::util::IOException);

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
     * 
     */
    const int BUFSIZE;

    char* buffer;

private:

    int bufhead;

    int buftail;

    Sample* samp;

    size_t sampDataToRead;

    char* sampDataPtr;

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
 * module. MessageSampleScanner uses the SampleScanner::readSamples
 * method to read pre-formatted dsm_samples from a IO device.
 */
class MessageSampleScanner: public SampleScanner
{
public:
    
    MessageSampleScanner();

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
     * A value of zero means the message length is variable, and
     * that a scanner is should always be looking for a
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
    void setMessageLength(int val)
    	throw(nidas::util::InvalidParameterException) 
    {
	messageLength = val;
    }

    int getMessageLength() const
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
 * separation parameters and reads the raw sensor output
 * from an IO device, separating messages into samples,
 * by recognizing separators in the message stream.  Timetags
 * are applied to the samples based on the time of receipt of
 * each block of data, corrected backwards by the computed
 * tranmission time of each byte in the block.
 * @see setUsecsPerByte().
 */
class MessageStreamScanner: public MessageSampleScanner
{
public:
    
    MessageStreamScanner();

    ~MessageStreamScanner();

    void init();

    void setNullTerminate(bool val) 
    {
        nullTerminate = val;
    }

    bool getNullTerminate()  const
    {
        return nullTerminate;
    }

    dsm_time_t readSamples(DSMSensor* sensor,SampleDater* dater)
    	throw (nidas::util::IOException)
    {
        if (getMessageSeparatorAtEOM()) return readSamplesSepEOM(sensor,dater);
        else return readSamplesSepBOM(sensor,dater);
    }

protected:
    /**
     * Method to read input and break it into samples
     * where the message separator occurs at the beginning of
     * the message.
     */
    dsm_time_t readSamplesSepBOM(DSMSensor* sensor, SampleDater* dater)
    	throw (nidas::util::IOException);

    /**
     * Method to read input and break it into samples
     * where the message separator occurs at the end of
     * the message.
     */
    dsm_time_t readSamplesSepEOM(DSMSensor* sensor, SampleDater* dater)
    	throw (nidas::util::IOException);

private:

    const size_t MAX_MESSAGE_STREAM_SAMPLE_SIZE;

    SampleT<char>* osamp;

    int separatorCnt;

    size_t outSampLen;

    char* outSampDataPtr;

    dsm_time_t bomtt;

    int sampleOverflows;

    size_t sampleLengthAlloc;

    bool nullTerminate;

};

}}	// namespace nidas namespace core

#endif
