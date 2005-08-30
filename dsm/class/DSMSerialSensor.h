/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef DSMSERIALSENSOR_H
#define DSMSERIALSENSOR_H

#include <dsm_serial.h>

#include <RTL_DSMSensor.h>
#include <atdUtil/InvalidParameterException.h>
#include <atdTermio/Termios.h>
#include <AsciiScanner.h>

namespace dsm {
/**
 * A sensor connected to a serial port.
 */
class DSMSerialSensor : public RTL_DSMSensor, public atdTermio::Termios {

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    DSMSerialSensor();

    ~DSMSerialSensor();

    /**
     * Override DSMSensor::getDefaultMode to allow writing.
     * @return One of O_RDONLY, O_WRONLY or O_RDWR.
     */
    int getDefaultMode() const { return O_RDWR; }

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(atdUtil::IOException,atdUtil::InvalidParameterException);

    void init() throw(atdUtil::InvalidParameterException);

    void addSampleTag(SampleTag* var)
    	throw(atdUtil::InvalidParameterException);
    /*
     * Close the device connected to the sensor.
     */
    void close() throw(atdUtil::IOException);

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
    void setMessageSeparator(const std::string& val) { msgsep = val; }
    const std::string& getMessageSeparator() const { return msgsep; }

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
     * for the separator.  This will not cause messages
     * to be truncated - all the data up to, and including
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

    /**
     * Is prompting active, i.e. isPrompted() is true, and startPrompting
     * has been called?
     */
    bool isPrompting() const { return prompting; }

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


    void startPrompting() throw(atdUtil::IOException);

    void stopPrompting() throw(atdUtil::IOException);

    void togglePrompting() throw(atdUtil::IOException);

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

    unsigned long getSampleId() const { return sampleId; }

    void printStatus(std::ostream& ostr) throw();

    /**
     * Process a raw sample, which in this case means do
     * a sscanf on the character string contents, creating
     * a processed sample of binary floating point data.
     */
    virtual bool process(const Sample*,std::list<const Sample*>& result)
    	throw();

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

    static std::string replaceEscapeSequences(std::string str);

protected:

    dsm_sample_id_t sampleId;

    std::string msgsep;
    bool sepAtEOM;
    int messageLength;
    std::string prompt;
    enum irigClockRates promptRate;
   
    std::list<AsciiScanner*> scanners;

    std::list<AsciiScanner*>::iterator nextScanner;

    int maxScanfFields;

    char* parsebuf;

    int parsebuflen;

    int scanfFailures;

    int scanfPartials;

    bool prompted;

    bool prompting;

    /**
     * Sensor latency, in seconds.
     */
    float latency;
};

}

#endif
