/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-11-19 21:42:40 -0700 (Sat, 19 Nov 2005) $

    $LastChangedRevision: 3128 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/CharacterSensor.h $

 ******************************************************************
*/

#ifndef NIDAS_CORE_CHARACTERSENSOR_H
#define NIDAS_CORE_CHARACTERSENSOR_H

#include <nidas/core/DSMSensor.h>
#include <nidas/core/AsciiSscanf.h>
#include <nidas/core/Sample.h>

namespace nidas { namespace core {

/**
 * Implementation of support for a sensor which outputs
 * a message stream of data - for example a sensor
 */
class CharacterSensor: public DSMSensor {

public:

    /**
     * No arg constructor.
     */
    CharacterSensor();

    virtual ~CharacterSensor();

    bool isRTLinux() const;

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner();

    void open(int flags) throw(nidas::util::IOException,
    	nidas::util::InvalidParameterException);

    void init() throw(nidas::util::InvalidParameterException);

    void addSampleTag(SampleTag* var)
    	throw(nidas::util::InvalidParameterException);

    /**
     * The messageSeparator is the string of bytes that sensor
     * generates to separate messages.
      */
    void setMessageSeparator(const std::string& val)
        throw(nidas::util::InvalidParameterException)
    {
        messageSeparator = DSMSensor::replaceBackslashSequences(val);
    }

    /**
     * Get message separator with backslash sequences replaced by their
     * intended character.
     */
    const std::string& getMessageSeparator() const
    {
	return messageSeparator;
    }

    /**
     * Get message separator with backslash sequences added back.
     */
    const std::string getBackslashedMessageSeparator() const
    {
        return DSMSensor::addBackslashSequences(messageSeparator);
    }

    /**
     * Is the message separator at the end of the message (true),
     * or at the beginning (false)?
     */
    void setMessageSeparatorAtEOM(bool val)
        throw(nidas::util::InvalidParameterException)
    {
	separatorAtEOM = val;
    }

    bool getMessageSeparatorAtEOM() const
    {
        return separatorAtEOM;
    }

    /**
     * Set the message length for this sensor, a zero or positive value.
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

    /**
     * Set the prompt string for this sensor.
     * The prompt string may contain backslash escape sequences.
     */
    virtual void setPromptString(const std::string& val)
    {
        prompt = val;
	if (prompt.length() > 0 && promptRate > 0.0)
		prompted = true;
    }

    /**
     * Is this a prompted sensor.  Will be true if setPromptString()
     * has been called with a non-empty string, and setPromptRate()
     * has been called with a rate other than IRIG_ZERO_HZ.
     */
    virtual bool isPrompted() const { return prompted; }

    virtual const std::string& getPromptString() const {
        return prompt;
    }

    /**
     * Set the rate at which prompts are sent to this sensor.
     * @param val An enumerated value, indicating the prompt rate.
     * Use the function irigClockRateToEnum(int rate) in irigclock.h
     * to convert a rate in Hertz to an enumerated value.
     */
    virtual void setPromptRate(float val) throw(nidas::util::InvalidParameterException)
    {
        promptRate = val;
	if (prompt.length() > 0 && promptRate > 0.0)
		prompted = true;
    }

    virtual float getPromptRate() const { return promptRate; }

    /**
     * Is prompting active, i.e. isPrompted() is true, and startPrompting
     * has been called?
     */
    virtual bool isPrompting() const { return false; }

    virtual void startPrompting() throw(nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),
		"startPrompting","not supported");
    }

    virtual void stopPrompting() throw(nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),
		"stopPrompting","not supported");
    }

    virtual void togglePrompting() throw(nidas::util::IOException)
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

    virtual void sendInitString() throw(nidas::util::IOException);

    bool getNullTerminated() const 
    {
        if (!getSampleScanner()) return false;
	return getSampleScanner()->getNullTerminate();
    }

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

    const std::list<AsciiSscanf*>& getScanfers() const
    {
        return sscanfers;
    }

    /**
     * Process a raw sample, which in this case means do
     * a sscanf on the character string contents, creating
     * a processed sample of binary floating point data.
     */
    virtual bool process(const Sample*,std::list<const Sample*>& result)
    	throw();

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);


private:

    mutable int rtlinux;

    std::string messageSeparator;

    bool separatorAtEOM;

    int messageLength;

    std::string prompt;

    float promptRate;
   
    std::list<AsciiSscanf*> sscanfers;

    std::list<AsciiSscanf*>::iterator nextSscanfer;

    int maxScanfFields;

    int scanfFailures;

    int scanfPartials;

    bool prompted;

    /**
     * String that is sent once after sensor is opened.
     */
    std::string initString;

    std::string emptyString;

};

}}	// namespace nidas namespace core

#endif
