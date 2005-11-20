/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef MESSAGESTREAMSENSOR_H
#define MESSAGESTREAMSENSOR_H

#include <DSMSensor.h>
#include <atdUtil/InvalidParameterException.h>
#include <AsciiScanner.h>
#include <Sample.h>

#include <irigclock.h>

namespace dsm {
/**
 * Implementation of support for a sensor which outputs
 * a message stream of data - for example a sensor
 */
class MessageStreamSensor {

public:

    /**
     * No arg constructor.
     */
    MessageStreamSensor();

    virtual ~MessageStreamSensor();

    void init() throw(atdUtil::InvalidParameterException);

    void addSampleTag(SampleTag* var)
    	throw(atdUtil::InvalidParameterException);

    /**
     * Set the message separator string for this sensor.
     * The separator string may contain backslash escape sequences:
     *	\\n=newline, \\r=carriage-return, \\t=tab, \\\\=backslash
     *  \\xhh=hex, where hh are (exactly) two hex digits and
     *  \\000=octal, where 000 are exactly three octal digits.
     */
     /* note that the above back slashes above are doubled so that
      * doxygen displays them as one back slash.  One does
      * not double them in the parameter string.
      */
    void setMessageSeparator(const std::string& val);

    /**
     * Get message separator with backslash sequences replaced by their
     * intended character.
     */
    const std::string& getMessageSeparator() const;

    /**
     * Get message separator with backslash sequences added back.
     */
    const std::string getBackslashedMessageSeparator() const;

    /**
     * Set a boolean, indicating whether the message separator
     * is at the end of the message (val=true), or at the
     * beginning (val=false).
     */
    void setMessageSeparatorAtEOM(bool val) { sepAtEOM = val; }
    bool getMessageSeparatorAtEOM() const { return sepAtEOM; }

    /**
     * Set the message length.
     * \param val The message length in bytes.  0 means the length
     *            is variable, or not important.
     * Setting the message length is only necessary if
     * there is a chance that the message separator could
     * be found in the data portion of the message, which
     * might be possible if the sensor is generating
     * binary data.  A positive message length causes the
     * parser to skip val number of bytes before searching
     * for the separator.  If this value is too small it
     * will not necessarily cause messages to be truncated -
     * all the data up to, and including
     * the separator is still read and passed along for
     * processing.  If the value is larger than the actual
     * message length it will cause two or more messages
     * to be concatenated into one.
     */

    void setMessageLength(int val) { messageLength = val; }
    int getMessageLength() const { return messageLength; }

    /**
     * Set the prompt string for this sensor.
     * The prompt string may contain backslash escape sequences:
     *	\\n=newline, \\r=carriage-return, \\t=tab, \\\\=backslash
     *  \\xhh=hex, where hh are (exactly) two hex digits and
     *  \\000=octal, where 000 are exactly three octal digits.
     */
    void setPromptString(const std::string& val) { prompt = val; }

    /**
     * Is this a prompted sensor.  Will be true if setPromptString()
     * has been called with a non-empty string, and setPromptRate()
     * has been called with a rate other than IRIG_ZERO_HZ.
     */
    bool isPrompted() const { return prompted; }

    const std::string& getPromptString() const {
        return prompt;
    }

    /**
     * Set the rate at which prompts are sent to this sensor.
     * @param val An enumerated value, indicating the prompt rate.
     * Use the function irigClockRateToEnum(int rate) in irigclock.h
     * to convert a rate in Hertz to an enumerated value.
     */
    void setPromptRate(enum irigClockRates val) { promptRate = val; }

    enum irigClockRates getPromptRate() const { return promptRate; }

    /**
     * Is prompting active, i.e. isPrompted() is true, and startPrompting
     * has been called?
     */
    bool isPrompting() const { return false; }

    virtual void startPrompting() throw(atdUtil::IOException)
    {
        throw atdUtil::IOException(getSensorName(),
		"startPrompting","not supported");
    }

    virtual void stopPrompting() throw(atdUtil::IOException)
    {
        throw atdUtil::IOException(getSensorName(),
		"stopPrompting","not supported");
    }

    virtual void togglePrompting() throw(atdUtil::IOException)
    {
	if (isPrompting()) stopPrompting();
	else startPrompting();
    }

    /**
     * Set the initialization string(s) for this sensor.
     * The init string may contain backslash escape sequences 
     * like the prompt string.
     */
    void setInitString(const std::string& val) { initString = val; }

    const std::string& getInitString() const { return initString; }

    /**
     * Set desired latency, providing some control
     * over the response time vs buffer efficiency tradeoff.
     * Setting a latency of 1/10 sec means buffer
     * data in the driver for a 1/10 sec, then send the data
     * to user space. As implemented here, it must be
     * set before doing a sensor open().
     * @param val Latency, in seconds.
     */
    void setLatency(float val) throw(atdUtil::InvalidParameterException)
    {
        latency = val;
    }

    float getLatency() const { return latency; }

    /**
     * How many samples have resulted in complete scanf failures -
     * nothing parsed, because the sensor messages do not
     * correspond to the sscanf format.
     */
    int getNumScanfFailures() const { return scanfFailures; }

    /**
     * How many samples have been partially scanned, because
     * a character in the middle of a message conflicts
     * with the sscanf format.
     */
    int getNumScanfPartials() const { return scanfPartials; }

    const std::list<AsciiScanner*>& getScanners() const
    {
        return scanners;
    }

    bool isNullTerminated() const { return nullTerminated; }

    void setNullTerminated(bool val) { nullTerminated = val; }

    /**
     * Process a raw sample, which in this case means do
     * a sscanf on the character string contents, creating
     * a processed sample of binary floating point data.
     */
    virtual bool scanMessageSample(const Sample*,std::list<const Sample*>& result)
    	throw();

    dsm_time_t readSamplesSepBOM(SampleDater* dater,
            DSMSensor* sensor) throw (atdUtil::IOException);

    dsm_time_t readSamplesSepEOM(SampleDater* dater,
            DSMSensor* sensor) throw (atdUtil::IOException);


    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);
    static std::string replaceBackslashSequences(std::string str);

    static std::string addBackslashSequences(std::string str);

protected:

    void setSensorName(const std::string& val) { name = val; }

private:

    const std::string& getSensorName() const { return name; }

    std::string name;

    std::string separatorString;

    bool sepAtEOM;

    int messageLength;

    std::string prompt;

    enum irigClockRates promptRate;
   
    std::list<AsciiScanner*> scanners;

    std::list<AsciiScanner*>::iterator nextScanner;

    int maxScanfFields;

    char* parsebuf;

    size_t parsebuflen;

    int scanfFailures;

    int scanfPartials;

    bool prompted;

    /**
     * Sensor latency, in seconds.
     */
    float latency;

    /**
     * Whether sample strings, as they come from the driver,
     * are null terminated.
     */
    bool nullTerminated;

    /**
     * String that is sent once after sensor is opened.
     */
    std::string initString;

    const size_t BUFSIZE;

    const size_t MAX_MESSAGE_STREAM_SAMPLE_SIZE;

    char* buffer;

    int separatorCnt;

    int separatorLen;

    char* separator;

    SampleT<char>* osamp;

    size_t outSampLen;

    size_t sampleLengthEstimate;

    char* outSampDataPtr;

    dsm_time_t bomtt;

    int sampleOverflows;

    int usecsPerChar;

    size_t sampleLengthAlloc;

};

}

#endif
