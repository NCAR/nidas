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
        UnixIODevice(),_termios(),_rts485(false),_usecsperbyte(0)
    {}

    /**
     * Constructor, passing the name of the device. Does not open
     * the device.
     */
    SerialPortIODevice(const std::string& name):
        UnixIODevice(name),_termios(),_rts485(false),_usecsperbyte(0)
    { }

    /**
     * Destructor. Does not close the device.
     */
    ~SerialPortIODevice() {}

    /**
     * open the device.
     */
    void open(int flags) throw(nidas::util::IOException);

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

    /**
     * Is this a RS485 half-duplex device, and if so, can the transmitter on the
     * data system be enabled/disabled by setting/clearing RTS?
     * If so, then RTS will be cleared when the device is opened, and set while
     * writing to the device.
     *
     * If is often the case that one must disable the local transmitter
     * in order to receive characters over half-duplex 485, otherwise
     * the transmitter clobbers the signal levels. On serial interfaces
     * that support rs232,422 and 485 protocols, it is common that
     * that the transmitter can be enabled/disabled with RTS from the
     * UART when the interface is configured for 485 mode.
     *
     * This RTS control of the transmitter in 485 mode is available on
     * port 4 of Eurotech Vipers and Titans, and on all ports of
     * the Diamond Emerald cards.
     *
     * The best situation is if automatic control of RTS on 485 accesses
     * is supported on the UART and supported for that UART in the kernel.
     * This provides precise timing, so that the transmitter is enabled
     * precisely while bit transmits are taking place.
     *
     * Automatic RTS control is available on the Exar XR16C285x UARTS of
     * the Eurotech Vipers and Titans. It is not available on the ST16C554
     * on the Diamond Emerald serial card. However, I don't see kernel
     * support for this on Exars in the mainline kernel, even as
     * recent as 3.8.2. Patches are floating around.
     *
     * If automatic RTS control is available in the kernel,
     * there will be ioctl macros TIOCGRS485/TIOCSRS485  in
     * /usr/include/asm-generic/ioctls.h, and struct serial_rs485 in
     * /usr/include/linux/serial.h.  Presumably if the request is not
     * supported on a given UART, you'll get an EINVAL. 
     *
     * See the kernel documentation:  Documentation/serial/serial-rs485.txt 
     *
     * Since kernel support is not available for this on the kernels and
     * UARTs we are using, code for the above ioctls is not yet in NIDAS.
     *
     * Without hardware and kernel support for automatic RTS control with 485,
     * we have to resort to inexact control of RTS from user space code.
     * This will probably result in loss of data if the remote device answers
     * back too quickly after a write to the device. Since this software
     * control is not great, 485  is best used for read-only devices.
     * Use 232 or 422 if you need read/write.
     *
     */
    void setRTS485(bool val) throw(nidas::util::IOException)
    {
        _rts485 = val;
        if (_rts485 && _fd >= 0) {
            int bits = TIOCM_RTS;
            // clear RTS
            if (::ioctl(_fd, TIOCMBIC, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIC",errno);
            _usecsperbyte = getUsecsPerByte();
        }
    }

    /**
     * Write to the device.
     */
    size_t write(const void *buf, size_t len) throw(nidas::util::IOException)
    {
	ssize_t result;
        int bits;


        if (_rts485) {
            // see the above discussion about RTS and 485. Here we
            // try an in-exact set/clear of RTS on either side of a write.
            bits = TIOCM_RTS;
            // set RTS before write
            if (_fd >= 0 && ::ioctl(_fd, TIOCMBIS, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIS",errno);
        }

        if ((result = ::write(_fd,buf,len)) < 0)
		throw nidas::util::IOException(getName(),"write",errno);

        if (_rts485) {
            // sleep until we think the last bit has been transmitted,
            // then clear RTS
            ::usleep(len * _usecsperbyte);
            if (_fd >= 0 && ::ioctl(_fd, TIOCMBIC, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIC",errno);
        }
	return result;
    }



protected:

    nidas::util::Termios _termios;

    bool _rts485;

    unsigned int _usecsperbyte;

};

}}	// namespace nidas namespace core

#endif
