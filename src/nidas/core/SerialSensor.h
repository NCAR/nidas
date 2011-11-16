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
#ifndef NIDAS_DYNLD_SERIALSENSOR_H
#define NIDAS_DYNLD_SERIALSENSOR_H

#include <nidas/core/CharacterSensor.h>
#include <nidas/core/LooperClient.h>
#include <nidas/core/SerialPortIODevice.h>

namespace nidas { namespace core {

using namespace nidas::core;

/**
 * Support for a sensor that is sending packets on a TCP socket, a UDP socket, a
 * Bluetooth RF Comm socket, or a good old RS232/422/485 serial port.
 * A SerialSensor builds the appropriate IODevice depending on the prefix
 * of the device name, see buildIODevice() below.
 * A SerialSensor also creates a SampleScanner, depending on the device name.
 * 
 * Configuration and opening of a SerialSensor is done in the following sequence:
 * 1.  After the configuration XML is being parsed, an instance of SerialSensor() is
 *     created, and the virtual method fromDOMElement() is called.
 *     SerialPort::fromDOMElement() configures the nidas::util::Termios of this SerialSensor
 *     from the attributes of the sensor DOM element.  fromDOMElement() also calls
 *     CharacterSensor::fromDOMElement(), which calls setMessageParameters().
 *     SerialSensor::setMessageParameters() further updates the Termios for raw
 *     character transfers.  Note that the device is not open yet.
 * 2.  virtual method SerialSensor::open is called, which calls CharacterSensor::open().
 *     CharacterSensor::open calls DSMSensor::open().
 *     DSMSensor::open() does the following, calling virtual methods:
 *         iodev = buildIODevice();
 *             Creates the appropriate IODevice, see SerialSensor::buildIODevice().
 *         iodev->open()
 *             If the IODevice is a SerialPortIODevice, then SerialPortIODevice::open()
 *             calls UnixIODevice::open() which opens the device file.
 *             SerialPortIODevice::open() then applies the Termios to the opened file descriptor.
 *         scanr = buildSampleScanner();
 *              Calls CharacterSensor::buildSampleScanner().
 *         scanr->init()
 */
class SerialSensor : public CharacterSensor
{

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    SerialSensor();

    ~SerialSensor();

    /**
     * Expose the Termios. One must call applyTermios() to
     * apply any changes to the serial port.
     */
    nidas::util::Termios& termios() { return _termios; }

    /**
     * Get a read-only copy of the Termios.
     */
    const nidas::util::Termios& getTermios() const { return _termios; }

    /**
     * Calls CharacterSensor::buildSampleScanner(), and then sets the 
     * per-byte transmission delay for that scanner:
     * SampleScanner::setUsecsPerByte().
     */
    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Creates an IODevice depending on the device name prefix:
     * name prefix      type of IODevice
     * inet:            TCPSocketIODevice
     * sock:            TCPSocketIODevice
     * usock:           UDPSocketIODevice
     * btspp:           BluetoothRFCommSocketIODevice
     * all others       SerialPortIODevice
     *
     * If a SerialPortIODevice is created, the Termios of this SerialSensor is
     * copied to the device, which will then be applied when the device is opened.
     */
    IODevice* buildIODevice() throw(nidas::util::IOException);

    /**
     * Open the device connected to the sensor. This calls
     * CharacterSensor::open(), and then sets up the port
     * prompting if it is required.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    /*
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

    /**
     * If the underlying IODevice is a SerialPortIODevice, update
     * the current Termios to the device.
     */
    void applyTermios() throw(nidas::util::IOException);

    /**
     * Set message separator and message length parameters, which are used to
     * parse and time-tag samples from the IODevice.
     */
    void setMessageParameters(unsigned int len,const std::string& sep, bool eom)
        throw(nidas::util::InvalidParameterException,nidas::util::IOException);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Is prompting active, i.e. isPrompted() is true, and startPrompting
     * has been called?
     */
    bool isPrompting() const { return _prompting; }

    /**
     * Start the prompters. They can be started and stopped multiple
     * times once a SerialSensor is opened.
     */
    void startPrompting() throw(nidas::util::IOException);

    void stopPrompting() throw(nidas::util::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
    	throw(nidas::util::InvalidParameterException);

    /**
     * If the underlying IODevice is a SerialPortIODevice,
     * return the value of SerialPortIODevice::getUsecsPerByte(),
     * otherwise return 0, which means no timetag correction
     * for transmission delay will be applied.
     */
    int getUsecsPerByte() const;

protected:

    /**
     * Perform whatever is necessary to initialize prompting right
     * after the device is opened.
     */
    void initPrompting() throw(nidas::util::IOException);

    /**
     * Shutdown prompting, typically done when a device is closed.
     */
    void shutdownPrompting() throw(nidas::util::IOException);

    void unixDevInit(int flags)
    	throw(nidas::util::IOException);

private:

    /**
     * Serial I/O parameters.
     */
    nidas::util::Termios _termios;

    /**
     * Non-null if the underlying IODevice is a SerialPortIODevice.
     */
    SerialPortIODevice* _serialDevice;

    class Prompter: public nidas::core::LooperClient
    {
    public:
        Prompter(SerialSensor* sensor): _sensor(sensor),
		_prompt(0),_promptLen(0), _promptPeriodMsec(0) {}

        ~Prompter();

        void setPrompt(const std::string& val);
        const std::string getPrompt() const { return _prompt; }

        void setPromptPeriodMsec(const int);
        int getPromptPeriodMsec() const { return _promptPeriodMsec; }

        /**
         * Method called by Looper in order to send a prompt.
         */
        void looperNotify() throw();
    private:
        SerialSensor* _sensor;
        char* _prompt;
	int _promptLen;
        int _promptPeriodMsec;

        /** copy not necessary */
        Prompter(const Prompter&);

        /** assignment not necessary */
        Prompter& operator=(const Prompter&);
    };

    std::list<Prompter*> _prompters;

    bool _prompting;

    /** No copying. */
    SerialSensor(const SerialSensor&);

    /** No assignment. */
    SerialSensor& operator=(const SerialSensor&);

};

}}	// namespace nidas namespace core

#endif
