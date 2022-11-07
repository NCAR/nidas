// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#ifndef NIDAS_CORE_CHARACTERSENSOR_H
#define NIDAS_CORE_CHARACTERSENSOR_H

#include "DSMSensor.h"
#include "Prompt.h"
#include <nidas/util/util.h>

namespace nidas { namespace core {

class AsciiSscanf;
class Sample;
class TimetagAdjuster;

struct MessageConfig {
    MessageConfig(const int initLength, const char* cSep, bool initEom ) 
        : msgLength(initLength), sep(cSep), sepAtEnd(initEom) {}
    int msgLength;
    std::string sep;
    bool sepAtEnd;
};

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
     * Implementation of DSMSensor::validate for a Character Sensor.
     * Currently initializes all the prompts for this sensor
     */
    void validate() throw(nidas::util::InvalidParameterException);

    /**
     * Initialize the CharacterSensor instance for post-processing.
     * This should only be done after the CharacterSensor
     * has been initialized with fromDOMElement.
     * Calls validateSscanfs.
     */
    void init() throw(nidas::util::InvalidParameterException);

    /**
     * The messageSeparator is the string of bytes that sensor
     * generates to separate messages.
      */
    virtual void setMessageParameters(unsigned int length, const std::string& val, bool eom)
        throw(nidas::util::IOException,nidas::util::InvalidParameterException);
    virtual void setMessageParameters(const MessageConfig& rMsgCfg)
        throw(nidas::util::IOException,nidas::util::InvalidParameterException)
    {
        setMessageParameters(rMsgCfg.msgLength, rMsgCfg.sep, rMsgCfg.sepAtEnd);
    }

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
     * @name Prompts
     *
     * Methods for managing the Prompt instances attached to a sensor.
     *
     * A sensor can be assigned multiple prompts, where the very first prompt
     * is known as the primary prompt, mostly because originally a sensor
     * could only ever have one prompt.  A sensor always contains at least the
     * primary prompt, but it starts out empty and invalid.  setPrompt() sets
     * that first prompt, and getPrompt() returns it.  All calls to
     * addPrompt() add successive prompts.  The first prompt can always be
     * modified by setPrompt() without replacing any prompts added by
     * addPrompt().  Generally, the first `<prompt>` element in a sensor is
     * assigned to the primary prompt slot.  All subsequent `<prompt>`
     * elements are added with addPrompt(), followed by all prompts within
     * sample tags.  Any prompts which are not valid, including the default
     * unmodified primary prompt, are not assigned to any prompters and will
     * not be used.
     */
    /** @{ */

    /**
     * @brief Set the primary sensor prompt.
     *
     * The primary prompt on a sensor is the first `<prompt>` element found in
     * a `<sensor>` XML element.  More prompts can be added with addPrompt().
     * If the primary prompt is never set or is not valid, then it will not be
     * used to prompt the sensor.  However, the primary prompt can be used to
     * inherit a rate for subsequent prompts.  The notion of a primary prompt
     * is mostly historical, since the same effect can also be achieved by
     * adding a single prompt with addPrompt().
     */
    void setPrompt(const Prompt& prompt);

    /**
     * @brief Return the primary prompt for this sensor.
     *
     * @return const Prompt& 
     */
    const Prompt& getPrompt() const;

    /**
     * Prompting Sensors can have multiple prompts and rates.  Add another
     * prompt and rate to this sensor.  If the prompt has a string but no
     * rate, then the rate will be set from the primary prompt before being
     * added.
     */
    virtual void addPrompt(const Prompt& prompt);

    /**
     * @brief Return a list of all Prompts attached to this sensor.
     *
     * This includes the primary prompt and all other prompts either within
     * the sensor or within samples of this sensor.  The sample tag prompts
     * are not added until after validate() is called.  Since this list can
     * include invalid prompts, especially if the primary prompt is used to
     * set the rate but is not valid itself, this list may contain prompts
     * which should not be assigned to prompters.
     *
     * @return const std::list<Prompt>& 
     */
    const std::list<Prompt>& getPrompts() const;

    /**
     * Is this a prompted sensor.  Will be true if there are any
     * Prompt::valid() prompts set in the `<sensor>` element or sample tags.
     * See setPrompt() and addPrompt().
     */
    virtual bool isPrompted() const;

    /**
     * Is prompting active, i.e. isPrompted() is true, and startPrompting
     * has been called?
     */
    virtual bool isPrompting() const;

    virtual void startPrompting() throw(nidas::util::IOException);

    virtual void stopPrompting() throw(nidas::util::IOException);

    virtual void togglePrompting() throw(nidas::util::IOException);

    /** @} */

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

    /**
     * The maximum number of fields in any of the AsciiSscanfs for
     * this CharacterSensor. Prior to scanning a message, a sample
     * of this size must be allocated.
     */
    int getMaxScanfFields() const { return _maxScanfFields; }

    /**
     * Virtual method to check that the Sscanfs for this CharacterSensor
     * are OK.  The default implementation checks that the number of parse
     * fields in each sscanf parser matches the number of variables in
     * the associated output sample.
     */
    virtual void validateSscanfs() throw(nidas::util::InvalidParameterException);

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

    virtual int scanSample(AsciiSscanf* sscanf, const char* inputstr, 
			   float* data_ptr);

    /**
     * Search through the AsciiSscanf instances attached to this sensor,
     * looking for the next scanner which parses at least one variable from
     * the given raw sample.  The search picks up after the last scanner
     * which matched a sample.  The parsed values are added to a new
     * Sample, and a pointer to the new Sample is returned.  If stag_out is
     * non-null, it is set to the SampleTag pointer for the AsciiSscanf
     * which matched.  If no scanners match this sample, then this returns
     * null.  The returned Sample has a reference which must be freed by
     * the caller or passed on.  Any unparsed variables in the returned
     * sample are filled with NaN using trimUnparsed().  The time tag of
     * the new Sample is set to the time of raw sample @p samp.  No other
     * time tag adjustments or variable conversions are applied, that is up
     * to the caller.
     **/
    SampleT<float>*
    searchSampleScanners(const Sample* samp, SampleTag** stag_out=0) throw();

    /**
     * Apply TimetagAdjuster and lag adjustments to the timetag of the
     * given sample.
     **/
    void
    adjustTimeTag(SampleTag* stag, SampleT<float>* outs);

    std::map<const SampleTag*, TimetagAdjuster*> _ttadjusters;

private:

    std::string _messageSeparator;

    bool _separatorAtEOM;

    int _messageLength;

    std::list<Prompt> _prompts;

    std::list<AsciiSscanf*> _sscanfers;

    std::list<AsciiSscanf*>::const_iterator _nextSscanfer;

    int _maxScanfFields;

    int _scanfFailures;

    int _scanfPartials;

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
