// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/
#ifndef NIDAS_CORE_SERIALPORTIODEVICE_H
#define NIDAS_CORE_SERIALPORTIODEVICE_H

#include <nidas/core/UnixIODevice.h>
#include <nidas/util/Termios.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/IOTimeoutException.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#ifdef DEBUG
#include <iostream>
#endif

namespace nidas { namespace core {

/**
 * A serial port.
 */
class SerialPortIODevice : public UnixIODevice {

public:

    /**
     * Constructor. Does not open any actual device.
     */
    SerialPortIODevice():
        UnixIODevice(),_termios()
    {}

    /**
     * Constructor, passing the name of the device. Does not open
     * the device.
     */
    SerialPortIODevice(const std::string& name):
        UnixIODevice(name),_termios()
    { }

    /**
     * Destructor. Does not close the device.
     */
    ~SerialPortIODevice() {}

    /**
     * open the device.
     */
    void open(int flags) throw(nidas::util::IOException)
    {
        UnixIODevice::open(flags);
        applyTermios();
    }

    /**
     * Apply the current Termios to an opened port.
     */
    void applyTermios() throw(nidas::util::IOException)
    {
        _termios.apply(_fd,getName());
    }

    nidas::util::Termios& termios() { return _termios; }

   /**
     * Calculate the transmission time of each byte from this
     * serial port. For RS232/485/422 serial ports is it simply
     * calculated as
     *  (databits + stopbits + 1) / baudrate
     * converted to microseconds.  This value is used by
     * MessageStreamScanners to guess at the time that the
     * first byte of a buffer was sent, knowing
     * the arrival time of that buffer of characters.
     * In the absence of further information, this corrected
     * transmission time is then used as the sample time.
     *
     * This method checks if the underlying IODevice is a tty,
     * returning a value of 0 usecs if it is not a tty, such as for
     * a socket.  Therefore the IODevice should exist and be open
     * for this method to work correctly.
     * In Sensor::open(), the IODevice is built, then opened,
     * and then the SampleScanner is created, which is when
     * this method should be called.
     *
     * It will return 0 for socket devices.
     */
    int getUsecsPerByte() const;

protected:

    nidas::util::Termios _termios;

};

}}	// namespace nidas namespace core

#endif
