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
#include <SampleScanf.h>

namespace dsm {
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

    /**
     * Set the format to scan ASCII data.
     * @see SampleScanf.setFormat()
     */
    void setScanfFormat(const std::string& val)
    	throw(atdUtil::InvalidParameterException);

    const std::string& getScanfFormat();

    void addSampleClient(SampleClient*);

    void removeSampleClient(SampleClient*);

    void removeAllSampleClients();

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    static std::string replaceEscapeSequences(std::string str);

    std::string msgsep;
    bool sepAtEOM;
    int messageLength;
    std::string prompt;
    enum irigClockRates promptRate;

    SampleScanf* scanner;
    atdUtil::Mutex scannerLock;
};

}

#endif
