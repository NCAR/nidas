/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

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

/**
 * A sensor connected to a serial port.
 */
class DSMSerialSensor : public RTL_DSMSensor, public atdTermio::Termios {

public:

    DSMSerialSensor();

    DSMSerialSensor(const std::string& name);

    ~DSMSerialSensor();

    /**
     * open the sensor. This opens the associated RT-Linux FIFOs.
     */
    void open(int flags) throw(atdUtil::IOException);

    void close() throw(atdUtil::IOException);

    /**
     * Set the message separator string for this sensor.
     * The separator string may contain backslash escape sequences:
     *	\n=newline, \r=carriage-return, \t=tab, \\=backslash
     *  \xhh=hex, where hh are two hex digits (must have 2)
     *  \000=octal, where 000 are three octal digits.
     */
    void setMessageSeparator(const std::string& val) { msgsep = val; }
    const std::string& getMessageSeparator() const { return msgsep; }

    void setMessageSeparatorAtEOM(bool val) { sepAtEOM = val; }
    bool getMessageSeparatorAtEOM() const { return sepAtEOM; }

    void setMessageLength(int val) { messageLength = val; }
    int getMessageLength() const { return messageLength; }

    /**
     * Set the prompt string for this sensor.
     * The prompt string may contain backslash escape sequences:
     *	\n=newline, \r=carriage-return, \t=tab, \\=backslash
     *  \xhh=hex, where hh are two hex digits (must have 2)
     *  \000=octal, where 000 are three octal digits.
     */
    void setPromptString(const std::string& val) { prompt = val; }

    const std::string& getPromptString() const {
        return prompt;
    }

    void setPromptRate(enum irigClockRates val) { promptRate = val; }
    enum irigClockRates getPromptRate() const { return promptRate; }

    void fromDOMElement(const XERCES_CPP_NAMESPACE::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    XERCES_CPP_NAMESPACE::DOMElement*
    	toDOMParent(XERCES_CPP_NAMESPACE::DOMElement* parent)
		throw(XERCES_CPP_NAMESPACE::DOMException);

    XERCES_CPP_NAMESPACE::DOMElement*
    	toDOMElement(XERCES_CPP_NAMESPACE::DOMElement* node)
		throw(XERCES_CPP_NAMESPACE::DOMException);

protected:
    std::string replaceEscapeSequences(std::string str);

    std::string msgsep;
    bool sepAtEOM;
    int messageLength;
    std::string prompt;
    enum irigClockRates promptRate;
};

#endif
