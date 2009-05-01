/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#ifndef NIDAS_CORE_CHARACTERSENSOR_H
#define NIDAS_CORE_CHARACTERSENSOR_H

#include <nidas/core/DSMSensor.h>
#include <nidas/core/AsciiSscanf.h>
#include <nidas/core/Sample.h>
#include <nidas/core/Prompt.h>
#include <nidas/util/util.h>

namespace nidas { namespace core {

/**
 * Implementation of support for a sensor which generates 
 * character output. Typically this character output is
 * somewhat free-form and is then parsed in the process()
 * method.
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

    /**
     * Open the sensor device port for real-time sampling.
     * This should only be done after the CharacterSensor
     * has been initialized with fromDOMElement.
     */
    void open(int flags) throw(nidas::util::IOException,
    	nidas::util::InvalidParameterException);

    /**
     * Initialize the CharacterSensor instance for post-processing.
     * This should only be done after the CharacterSensor
     * has been initialized with fromDOMElement.
     */
    void init() throw(nidas::util::InvalidParameterException);

    /**
     * The messageSeparator is the string of bytes that sensor
     * generates to separate messages.
      */
    void setMessageSeparator(const std::string& val)
        throw(nidas::util::InvalidParameterException);

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
        return nidas::util::addBackslashSequences(messageSeparator);
    }

    /**
     * Is the message separator at the end of the message (true),
     * or at the beginning (false)?
     */
    void setMessageSeparatorAtEOM(bool val)
        throw(nidas::util::InvalidParameterException);

    bool getMessageSeparatorAtEOM() const
    {
        return separatorAtEOM;
    }

    /**
     * Set the message length for this sensor, a zero or positive value.
     */
    void setMessageLength(int val)
        throw(nidas::util::InvalidParameterException);

    int getMessageLength() const
    {
	return messageLength;
    }

    /**
     * Prompting Sensors can have multiple prompts and rates.
     * Add another prompt and rate to this sensor.
     * The prompt string may contain backslash excape sequences.
     * @param promptRate An enumerated value, indicating the prompt rate.
     * Use the function irigClockRateToEnum(int rate) in irigclock.h
     * to convert a rate in Hertz to an enumerated value.
     */
    virtual void addPrompt(const std::string& promptString, const float promptRate)
    {
        Prompt prompt;
        prompt.setString(promptString);
        prompt.setRate(promptRate);

        _prompts.push_back(prompt);
        _prompted = true;
//cerr<< "pushed back prompt.  String = "<<promptString<<" rate= "<<promptRate;
    }

    const std::list<Prompt>& getPrompts() const { return _prompts;}

    /**
     * Is this a prompted sensor.  Will be true if setPromptString()
     * has been called with a non-empty string, and setPromptRate()
     * has been called with a rate other than IRIG_ZERO_HZ.
     */
    virtual bool isPrompted() const { return _prompted; }

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

    /**
     * Return the list of AsciiSscanfs requested for this CharacterSensor.
     * This list is only valid after the init() method has been called.
     */
    const std::list<AsciiSscanf*>& getScanfers() const
    {
        return sscanfers;
    }

    int getMaxScanfFields() const { return maxScanfFields; }

    /**
     * Process a raw sample, which in this case means do
     * a sscanf on the character string contents, creating
     * a processed sample of binary floating point data.
     */
    virtual bool process(const Sample*,std::list<const Sample*>& result)
    	throw();

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

    /**
     * @return: true if this CharacterSensor as been configured to
     *          use one or more  AsciiSscanf objects to parse its data.
     */
    bool doesAsciiSscanfs();

protected:

    /**
     * Set the rate at which prompts are sent to this sensor.
     * This will be set on a CharacterSensor if a <prompt> element
     * is found for <sensor>, not as a sub-element of <sample>.
     */
    void setPromptRate(const float val) {_promptRate = val;}

    const float getPromptRate() { return (_promptRate);}

private:

    /**
     * Set the prompt string for this sensor.
     * The prompt string may contain backslash escape sequences.
     */
//    virtual void setPromptString(const std::string& val)
//    {
//        prompt = val;
//	if (prompt.length() > 0 && promptRate > 0.0)
//		_prompted = true;
//    }


    mutable int rtlinux;

    std::string messageSeparator;

    bool separatorAtEOM;

    int messageLength;

    std::list<Prompt> _prompts;
    //std::string prompt;

    float _promptRate;
   
    std::list<AsciiSscanf*> sscanfers;

    std::list<AsciiSscanf*>::iterator nextSscanfer;

    int maxScanfFields;

    int scanfFailures;

    int scanfPartials;

    bool _prompted;

    /**
     * String that is sent once after sensor is opened.
     */
    std::string initString;

    std::string emptyString;

};

}}	// namespace nidas namespace core

#endif
