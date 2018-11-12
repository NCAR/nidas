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

#ifndef MOCK_SERIALPORT_IODEVICE_H
#define MOCK_SERIALPORT_IODEVICE_H

#include "SerialPortIODevice.h"

#include "MockSerialXcvrCtrl.h"

using namespace nidas::util;
using namespace nidas::core;

/**
 *  A mock serial port and all associated configurations.
 */
class MockSerialPortIODevice : public SerialPortIODevice
{

public:

    /**
     * Constructor. Does not open any actual device.
     */
    MockSerialPortIODevice() : SerialPortIODevice() {}

    /**
     * Constructor, passing the name of the device. Does not open
     * the device.
     */
    MockSerialPortIODevice(const std::string& name, PortConfig initPortConfig)
    : SerialPortIODevice()
    {
        UnixIODevice::setName(name);
        setPortConfig(initPortConfig);
    }

    /**
     * Copy constructor.  The attributes of the port are copied,
     * but if the original is opened, the copy will not be
     * opened.
     */
    MockSerialPortIODevice(const MockSerialPortIODevice& /*rRight*/)
    : SerialPortIODevice() {}

    /**
     * Constructor, given a device name. The device is *NOT* opened, mainly
     * just to avoid throwing an exception in the constructor. Perhaps
     * that should be changed.
     */
    MockSerialPortIODevice(const std::string& name) : SerialPortIODevice(name) {}

    /**
     * For serial port that is already open (stdin for example).
     * */
    MockSerialPortIODevice(const std::string& name, int fd) : SerialPortIODevice(name, fd) {}

    /**
     * Does not close the file descriptor if is is open.
     */
    virtual ~MockSerialPortIODevice() {}

    /**
     * open the serial port. The current Termios settings
     * are also applied to the port.
     */
    void open(int flags = O_RDONLY) throw(nidas::util::IOException)
    {
        UnixIODevice::open(flags);
        applyPortConfig();
    }

    void flush() {};

    /* 
     * Check whether this serial port is using a device which needs port control
     */
    virtual void checkXcvrCtrlRequired(const std::string& /*name*/)
    {
        PortConfig thisPortConfig = getPortConfig();
        // it always is...
        setXcvrCtrl(new MockSerialXcvrCtrl(thisPortConfig.xcvrConfig.port,
                                           thisPortConfig.xcvrConfig.portType,
                                           thisPortConfig.xcvrConfig.termination,
                                           thisPortConfig.xcvrConfig.sensorPower));
    }

    /**
     *  Get the SerialXcvrCtrl object for direct updating
     */
    SerialXcvrCtrl* getXcvrCtrl() {return _pXcvrCtrl;}

//    void applyPortConfig();

    /**
     * Do a tcdrain() system call on the device. According to the tcdrain man page, it
     * "waits until all output written to the object referred to by fd has been transmitted".
     */
    void drain() {}

    /**
     * Do a tcflush() system call on the device. According to the tcflush man page, it
     * "discards data received but not read".
     */
    void flushInput() {}

    /**
     * Do a tcflush() system call on the device. According to the tcflush man page, it
     * "discards data written to the object referred to by fd but not transmitted".
     */
    void flushOutput() {}

    void flushBoth() {}

//    /**
//     * Read bytes until either the term character is read, or len-1 number
//     * of characters have been read. buf will be null terminated.
//     *
//     */
//    virtual int readUntil(char *buf,int len,char term);
//
//    /**
//     * Do a readUntil with a newline terminator.
//     */
//    virtual int readLine(char *buf,int len);
//
//    virtual int read(char *buf,int len, int timeout=0) throw(nidas::util::IOException);
//
//    virtual char readchar();
//
//    virtual std::size_t write(const void *buf, std::size_t len) throw(nidas::util::IOException);

//protected:
//
//    PortConfig _workingPortConfig;
//
//    SerialXcvrCtrl* _pXcvrCtrl;
//
//    unsigned int _usecsperbyte;
//
//    enum state { OK, TIMEOUT_OR_EOF} _state;
//
//    char *_savep;
//
//    char *_savebuf;
//
//    int _savelen;
//
//    int _savealloc;
//
//    bool _blocking;

private:
    /**
     * No assignment.
     */
    MockSerialPortIODevice& operator=(const MockSerialPortIODevice&);
};

#endif // MOCK_SERIALPORT_IODEVICE_H
