// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
#include <nidas/core/Prompt.h>
#include <nidas/util/util.h>

namespace nidas { namespace core {

class AsciiSscanf;
class Sample;

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

    /**
     * Creates an IODevice depending on the device name prefix:
     * name prefix      type of IODevice
     * inet:            TCPSocketIODevice
     * sock:            TCPSocketIODevice
     * usock:           UDPSocketIODevice
     * btspp:           BluetoothRFCommSocketIODevice
     * all others       UnixIODevice
     */
    IODevice* buildIODevice() throw(nidas::util::IOException);

    /**
     * Creates a SampleScanner for this DSMSensor depending on the
     * device name prefix:
     * name prefix      type of SampleScanner
     * usock:           DatagramSampleScanner
     * all others       MessageStreamScanner
     */
    SampleScanner* buildSampleScanner()
    	throw(nidas::util::InvalidParameterException);

    /**
     * Open the sensor device port for real-time sampling.
     * This should only be done after the CharacterSensor
     * has been initialized with fromDOMElement.
     */
    void open(int flags) throw(nidas::util::IOException,
    	nidas::util::InvalidParameterException);

    /**
     * Validate this Character Sensor - basically used to initialize all
     * the prompts for this sensor
     */
    void validate() throw(nidas::util::InvalidParameterException);

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
    virtual void setMessageParameters(unsigned int length, const std::string& val, bool eom)
        throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    /**
     * Get message separator with backslash sequences replaced by their
     * intended character.
     */
    const std::string& getMessageSeparator() const
    {
	return _messageSeparator;
    }

    /**
     * Get message separator with backslash sequences added back.
     */
    const std::string getBackslashedMessageSeparator() const
    {
        return nidas::util::addBackslashSequences(_messageSeparator);
    }

    bool getMessageSeparatorAtEOM() const
    {
        return _separatorAtEOM;
    }

    int getMessageLength() const
    {
	return _messageLength;
    }

    /**
     * Prompting Sensors can have multiple prompts and rates.
     * Add another prompt and rate to this sensor.
     * @param promptString May contain backslash excape sequences.
     * @param promptRate prompts/sec.
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
    void setInitString(const std::string& val) { _initString = val; }

    const std::string& getInitString() const { return _initString; }

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
    int getNumScanfFailures() const { return _scanfFailures; }

    /**
     * How many samples have been partially scanned, because
     * a character in the middle of a message conflicts
     * with the sscanf format.
     */
    int getNumScanfPartials() const { return _scanfPartials; }

    /**
     * Return the list of AsciiSscanfs requested for this CharacterSensor.
     * This list is only valid after the init() method has been called.
     */
    const std::list<AsciiSscanf*>& getScanfers() const
    {
        return _sscanfers;
    }

    int getMaxScanfFields() const { return _maxScanfFields; }

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
     * Set the rate at which <sensor> prompts are sent to this sensor.
     * This will be set on a CharacterSensor if a <prompt> element
     * is found for <sensor>, not as a sub-element of <sample>.
     */
    void setPromptRate(const float val) {_promptRate = val;}

    float getPromptRate() { return (_promptRate);}

    /**
     * Set the <sensor> prompt string for this sensor.
     * The prompt string may contain backslash escape sequences.
     */
    void setPromptString(const std::string& val) { _promptString = val; }

    const std::string& getPromptString() { return (_promptString);}

    virtual int scanSample(AsciiSscanf* sscanf, const char* inputstr, 
			   float* data_ptr);

private:

    std::string _messageSeparator;

    bool _separatorAtEOM;

    int _messageLength;

    std::list<Prompt> _prompts;

    std::string _promptString;

    float _promptRate;
   
    std::list<AsciiSscanf*> _sscanfers;

    std::list<AsciiSscanf*>::const_iterator _nextSscanfer;

    int _maxScanfFields;

    int _scanfFailures;

    int _scanfPartials;

    bool _prompted;

    /**
     * String that is sent once after sensor is opened.
     */
    std::string _initString;

    std::string _emptyString;

    /** No copying */
    CharacterSensor(const CharacterSensor&);

    /** No assignment */
    CharacterSensor& operator=(const CharacterSensor&);

};

}}	// namespace nidas namespace core

#endif
