// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_DYNLD_SERIALSENSOR_H
#define NIDAS_DYNLD_SERIALSENSOR_H

#include "CharacterSensor.h"
#include "LooperClient.h"
#include "SerialPortIODevice.h"

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
 *     Note that the device is not open yet.
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
     *
     * @throws nidas::util::InvalidParameterException
     **/
    SampleScanner* buildSampleScanner();

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
     *
     * @throws nidas::util::IOException
     **/
    IODevice* buildIODevice();

    /**
     * Open the device connected to the sensor. This calls
     * CharacterSensor::open(), and then sets up the port
     * prompting if it is required.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void open(int flags);

    /*
     * Close the device connected to the sensor.
     *
     * @throws nidas::util::IOException
     **/
    void close();

    /**
     * If the underlying IODevice is a SerialPortIODevice, update
     * the current Termios to the device.
     *
     * @throws nidas::util::IOException
     **/
    void applyTermios();

    /**
     * Set message separator and message length parameters, which are used to
     * parse and time-tag samples from the IODevice.
     *
     * @throws nidas::util::InvalidParameterException,nidas::util::IOException
     **/
    void setMessageParameters(unsigned int len,const std::string& sep, bool eom);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Is prompting active, i.e. isPrompted() is true, and startPrompting
     * has been called?
     */
    bool isPrompting() const { return _prompting; }

    /**
     * Start the prompters. They can be started and stopped multiple
     * times once a SerialSensor is opened.
     *
     * @throws nidas::util::IOException
     **/
    void startPrompting();

    /**
     * @throws nidas::util::IOException
     **/
    void stopPrompting();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement* node);

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
     *
     * @throws nidas::util::IOException
     **/
    void initPrompting();

    /**
     * Shutdown prompting, typically done when a device is closed.
     *
     * @throws nidas::util::IOException
     **/
    void shutdownPrompting();

    /**
     * @throws nidas::util::IOException
     **/
    void unixDevInit(int flags);

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
            _prompt(0),_promptLen(0), _promptPeriodMsec(0),
            _promptOffsetMsec(0) {}

        ~Prompter();

        void setPrompt(const std::string& val);
        const std::string getPrompt() const { return _prompt; }

        void setPromptPeriodMsec(const int);
        int getPromptPeriodMsec() const { return _promptPeriodMsec; }

        void setPromptOffsetMsec(const int);
        int getPromptOffsetMsec() const { return _promptOffsetMsec; }

        /**
         * Method called by Looper in order to send a prompt.
         */
        void looperNotify() throw();
    private:
        SerialSensor* _sensor;
        char* _prompt;
	int _promptLen;
        int _promptPeriodMsec;
        int _promptOffsetMsec;

        /** copy not necessary */
        Prompter(const Prompter&);

        /** assignment not necessary */
        Prompter& operator=(const Prompter&);
    };

    std::list<Prompter*> _prompters;

    bool _prompting;

    /**
     * Should the RTS line on this port be controled for half-duplex 485?
     * See SerialPortIODevice.h.
     */
    int _rts485;

    /** No copying. */
    SerialSensor(const SerialSensor&);

    /** No assignment. */
    SerialSensor& operator=(const SerialSensor&);

};

}}	// namespace nidas namespace core

#endif
