// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
 * Currently there are four implementations of SampleScanner, which
 * differ in how they extract samples from the internal buffer:
 *
 * DriverSampleScanner:
 *    A SampleScanner for reading samples that have been
 *    pre-formatted by a device driver - they already have a header
 *    consisting of a timetag, and a data length.
 *    This can be used, for example, to read samples from a driver
 *    supporting an A/D converter.
 *
 * MessageSampleScanner:
 *    Subclass of DriverSampleScanner, supporting the set/get
 *    of the message separation parameters.  This scanner does
 *    not itself break the input into samples, since this
 *    has been done by the driver module.
 *
 * MessageStreamScanner:
 *    Provides sets and gets of message separation parameters used in
 *    parsing input messages from a sensor. Parses the stream
 *    input, breaking it into samples, by recognizing
 *    separators in the message stream.
 *
 * DatagramSampleScanner:
 *    Creates samples from input datagrams, a simple task since the
 *    OS maintains the separation of the input datagrams. Each
 *    datagram becomes a separate sample, with a timetag taken from
 *    the system clock at the time the scanner has determined that
 *    there is data available.
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
     * Set the parameters associated with scanning of character messages.
     */
    virtual void setMessageParameters(unsigned int len, const std::string& val, bool eom)
    	throw(nidas::util::InvalidParameterException) = 0;

    /**
     * Returns an empty string.
     */
    virtual const std::string& getMessageSeparator() const 
    {
        return _emptyString;
    }

    /**
     * Returns an empty string.
     */
    virtual const std::string getBackslashedMessageSeparator() const
    {
	return _emptyString; 
    }

    virtual bool getMessageSeparatorAtEOM() const
    {
        return false;
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
        _usecsPerByte = val;
    }

    int getUsecsPerByte() const
    {
        return _usecsPerByte;
    }

    /**
     * Read from the sensor into the internal buffer of this
     * SampleScanner.
     */
    virtual size_t readBuffer(DSMSensor* sensor, bool& exhausted)
        throw(nidas::util::IOException);

    /**
     * Read from the sensor into the internal buffer of this
     * SampleScanner, providing a timeout in milliseconds.
     * This will throw nidas::util::IOTimeoutException
     * if the read fails due to a timeout.
     */
    virtual size_t readBuffer(DSMSensor* sensor, bool& exhausted,int msecTimeout)
        throw(nidas::util::IOException);

    virtual void clearBuffer();

    /**
     * Extract the next sample from the buffer. Returns
     * NULL if there are no more samples in the buffer.
     */
    virtual Sample* nextSample(DSMSensor* sensor) = 0;

    unsigned int getBytesInBuffer() const { return _bufhead - _buftail; }

    virtual void resetStatistics();

    /**
     * Update the sensor sampling statistics: samples/sec,
     * bytes/sec, min/max sample size, that can be accessed via
     * getObservedSamplingRate(), getObservedDataRate() etc.
     * Should be called every periodUsec by a user of this sensor.
     * @param periodUsec Statistics period.
     */
    virtual void calcStatistics(unsigned int periodUsec);

    unsigned int getMaxSampleLength() const
    	{ return _maxSampleLength[_reportIndex]; }

    unsigned int getMinSampleLength() const
    { 
        // if max is 0 then we haven't gotten any data
        if (_maxSampleLength[_reportIndex] == 0) return 0;
        return _minSampleLength[_reportIndex];
    }

    unsigned int getBadTimeTagCount() const
    {
	return _badTimeTags;
    }

    float getObservedSamplingRate() const;

    float getObservedDataRate() const;

    void addNumBytesToStats(size_t val) { _nbytes += val; }

    void addSampleToStats(unsigned int val)
    {
	_nsamples++;
        _minSampleLength[_currentIndex] =
		std::min(_minSampleLength[_currentIndex],val);
        _maxSampleLength[_currentIndex] =
		std::max(_maxSampleLength[_currentIndex],val);
    }

    void incrementBadTimeTags()
    {
        _badTimeTags++;
    }

protected:

    /**
     * Buffer size for reading from sensor.
     */
    const unsigned int BUFSIZE;

    char* _buffer;

    unsigned int _bufhead;

    unsigned int _buftail;

    Sample* _osamp;

    struct dsm_sample _header;

    unsigned int _outSampRead;
 
    unsigned int _outSampToRead;

    char* _outSampDataPtr;

    std::string _messageSeparator;

    int _messageLength;

    bool _separatorAtEOM;

    /**
     * messageSeparator in a C string.
     */
    char* _separator;

    /**
     * Length of messageSeparator.
     */
    int _separatorLen;

private:

    std::string _emptyString;

    time_t _initialTimeSecs;

    unsigned int _minSampleLength[2];

    unsigned int _maxSampleLength[2];

    int _currentIndex;

    int _reportIndex;

    size_t _nsamples;

    size_t _nbytes;

    unsigned int _badTimeTags;

    /**
    * Observed number of samples per second.
    */
    float _sampleRateObs;

    float _dataRateObs;

    int _usecsPerByte;

    /**
     * No copy (could be added if needed).
     */
    SampleScanner(const SampleScanner&);

    /**
     * No assignment (could be added if needed).
     */
    SampleScanner& operator=(const SampleScanner&);
};

/**
 * A SampleScanner for reading samples that have been
 * pre-formatted by a device driver - they already have a header
 * consisting of a timetag, and a data length.
 * This can be used, for example, to read samples from a driver
 * supporting an A/D converter.
 */
class DriverSampleScanner: public SampleScanner
{
public:

    DriverSampleScanner(int bufsize=8192);

    /**
     * setMessageSeparator is not implemented in DriverSampleScanner.
     * Throws nidas::util::InvalidParameterException.
     */
    void setMessageParameters(unsigned int, const std::string&, bool)
    	throw(nidas::util::InvalidParameterException)
    {
    	throw nidas::util::InvalidParameterException(
		"setMessageSeparator not supported");
    }

    /**
     * Extract the next sample from the buffer. Returns
     * NULL if there are no more samples in the buffer.
     */
    Sample* nextSample(DSMSensor* sensor);

private:

};

/**
 * A DriverSampleScanner which supports the set/get of message
 * separation parameters. Typically these parameters are
 * sent down to a driver module by an implementation of DSMSensor.
 */
class MessageSampleScanner: public DriverSampleScanner
{
public:
    
    MessageSampleScanner(int bufsize=8192);

    /**
     * Set the parameters which delineate a message for this scanner.
     * The messageSeparator is the string of bytes that a sensor
     * outputs between messages.  The string may contain
     * baskslash sequences.
     *
     * @param len The message length in bytes.
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
     * @param eom True means the message scanner expects the separator
     *        at the end of the message, and the next
     *        byte read after the separator is the first
     *        byte of the next message. The timetag of
     *        a sample is the receipt time of the first
     *        byte after the separator of the previous message.
     *        False means the scanner expects the separator
     *        at the beginning of the message, in which case
     *        the timetag of a sample is the receipt time of the
     *        first byte of the messageSeparator.
     * @see * nidas::util::replaceBackslashSequences()
     */
    void setMessageParameters(unsigned int len, const std::string& val, bool eom)
    	throw(nidas::util::InvalidParameterException);

    /**
     * Get message separator string. Any backslash sequences will have
     * been replaced by their intended value.
     */
    const std::string& getMessageSeparator() const
    {
        return _messageSeparator;
    }

    /**
     * Get message separator with backslash sequences added back.
     */
    const std::string getBackslashedMessageSeparator() const;

    /**
     * Is the message separator at the end of the message (true),
     * or at the beginning (false)?
     */
    bool getMessageSeparatorAtEOM() const
    {
        return _separatorAtEOM; 
    }

    unsigned int getMessageLength() const
    {
        return _messageLength;
    }

private:

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
class MessageStreamScanner: public SampleScanner
{
public:
    
    MessageStreamScanner(int bufsize=2048);

    void setMessageParameters(unsigned int len, const std::string& val, bool eom)
    	throw(nidas::util::InvalidParameterException);

    /**
     * Get message separator string. Any backslash sequences will have
     * been replaced by their intended value.
     */
    const std::string& getMessageSeparator() const
    {
        return _messageSeparator;
    }

    /**
     * Get message separator with backslash sequences added back.
     */
    const std::string getBackslashedMessageSeparator() const;

    bool getMessageSeparatorAtEOM() const
    {
        return _separatorAtEOM; 
    }

    unsigned int getMessageLength() const
    {
        return _messageLength;
    }

    void setNullTerminate(bool val) 
    {
        _nullTerminate = val;
    }

    bool getNullTerminate()  const
    {
        return _nullTerminate;
    }

    Sample* nextSample(DSMSensor* sensor)
    {
        return (this->*_nextSampleFunc)(sensor);
    }

    size_t readBuffer(DSMSensor* sensor, bool& exhausted)
        throw(nidas::util::IOException);

    size_t readBuffer(DSMSensor* sensor, bool& exhausted,int msecTimeout)
        throw(nidas::util::IOException);

protected:

    /**
     * Method to read input and break it into samples
     * where the message separator occurs at the beginning of
     * the message.
     */
    virtual Sample* nextSampleSepBOM(DSMSensor* sensor);

    /**
     * Method to read input and break it into samples
     * where the message separator occurs at the end of
     * the message.
     */
    virtual Sample* nextSampleSepEOM(DSMSensor* sensor);

    /**
     * Method to read input and break it into samples
     * strictly by record length.
     */
    Sample* nextSampleByLength(DSMSensor* sensor);

    /* ptr to setXXX member function for setting an attribute of this
     * class, based on the value of the tag from the IOStream.
     */
    Sample* (MessageStreamScanner::* _nextSampleFunc)(DSMSensor*);

    /**
     * If a new sample can be allocated without exceeding
     * MAX_MESSAGE_STREAM_SAMPLE_SIZE then get a new, larger sample,
     * copying the timetag, id and existing data from the current sample,
     * and return null. The data members _osamp, and _outSampDataPtr
     * are updated to point to the new sample.
     * Otherwise, since we've reached the MAX limit, more data will
     * not be added to the sample, so return a pointer to the existing
     * max'd out sample, which should then be returned as the value
     * of the nextSample() method.
     */
    Sample* requestBiggerSample(unsigned int nc);

protected:

    dsm_time_t _tfirstchar;

    const unsigned int MAX_MESSAGE_STREAM_SAMPLE_SIZE;

    int _separatorCnt;

    dsm_time_t _bomtt;

    int _sampleOverflows;

    /**
     * Size of samples to allocate.
     */
    unsigned int _sampleLengthAlloc;

    bool _nullTerminate;

    /**
     * Count of number of consecutive samples smaller than _sampleLengthAlloc.
     */
    int _nsmallSamples;
    
    /**
     * Number of bytes allocated in data portion of current output sample.
     */
    unsigned int _outSampLengthAlloc;

};

class DatagramSampleScanner: public SampleScanner
{
public:
    
    DatagramSampleScanner(int bufsize=8192);

    /**
     * setMessageSeparator is not implemented in DatagramSampleScanner.
     * Throws nidas::util::InvalidParameterException.
     */
    void setMessageParameters(unsigned int, const std::string&, bool)
    	throw(nidas::util::InvalidParameterException)
    {
    	throw nidas::util::InvalidParameterException(
		"setMessageParameters not supported");
    }

    /**
     * Read from the sensor into the internal buffer of this
     * SampleScanner.
     */
    size_t readBuffer(DSMSensor* sensor, bool& exhausted)
        throw(nidas::util::IOException);

    /**
     * Extract the next sample from the buffer. Returns
     * NULL if there are no more samples in the buffer.
     */
    Sample* nextSample(DSMSensor* sensor);

    /**
     * User of DatagramSampleScanner should specify if they want
     * the samples to be null terminated.  In general, if
     * this SampleScanner is used by a CharacterSensor with a
     * scanfFormat defined for one or more samples, then the
     * samples should be null terminated.
     */
    void setNullTerminate(bool val) 
    {
        _nullTerminate = val;
    }

    bool getNullTerminate()  const
    {
        return _nullTerminate;
    }


private:
    std::list<int> _packetLengths;

    std::list<dsm_time_t> _packetTimes;

    bool _nullTerminate;

};

}}	// namespace nidas namespace core

#endif
