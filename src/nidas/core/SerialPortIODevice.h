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

#ifndef NIDAS_CORE_SERIALPORTIODEVICE_H
#define NIDAS_CORE_SERIALPORTIODEVICE_H

#include "UnixIODevice.h"
#include "SerialPortPhysicalControl.h"
#include <nidas/util/Termios.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/Termios.h>
#include <nidas/util/IOException.h>


#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <iostream>
#include <sys/ioctl.h>

#ifdef DEBUG
#include <iostream>
#endif

namespace nidas { namespace core {

/**
 *  A serial port and all associated configurations. Typically these are enumerated by the 
 *  ftdi_sio kernel module at boot as /dev/ttyUSB[0...]. Standard unix termios operations 
 *  and sometimes IOCTL are used to set up various UART parameters like baud, num/start/stop 
 *  bits and so forth. 
 *  
 *  In the past, the serial drivers line were configured for the port type prior to 
 *  deployment via manually installed jumpers. Now there is GPIO to manage this task, and so 
 *  serial ports need to configure the serial line drivers to support the desired serial 
 *  port type, termination and power status. 
 */class SerialPortIODevice : public UnixIODevice
{

public:

    /**
     * Constructor. Does not open any actual device.
     */
    SerialPortIODevice();

    /**
     * Constructor, passing the name of the device. Does not open
     * the device.
     */
    SerialPortIODevice(const std::string& name, const PORT_TYPES portType = RS232, const TERM term=NO_TERM);

    /**
     * Copy constructor.  The attributes of the port are copied,
     * but if the original is opened, the copy will not be
     * opened.
     */
    SerialPortIODevice(const SerialPortIODevice&);

    /**
     * Constructor, given a device name. The device is *NOT* opened, mainly
     * just to avoid throwing an exception in the constructor. Perhaps
     * that should be changed.
     */
    SerialPortIODevice(const std::string& name);

    /**
     * For serial port that is already open (stdin for example).
     * */
    SerialPortIODevice(const std::string& name, int fd);

    /**
     * Does not close the file descriptor if is is open.
     */
    virtual ~SerialPortIODevice();

    /* Some constructors don't set the name right away. So set it
     * and then check whether or not it needs a port control object.
     */
    virtual void setName(const std::string& name)
    {
        UnixIODevice::setName(name);
        checkPortControlRequired(name);
    }

    /**
     * Writable reference to the SerialPortIODevice's Termios.
     * If the SerialPortIODevice is open, the user should call
     * applyTermios() for any modifications to take effect.
     */
    nidas::util::Termios& termios() { return _termios; }
    
    /**
     * Readonly reference to Termios.
     */
    const nidas::util::Termios& getTermios() const { return _termios; }

    /**
     * Apply the Termios settings to an opened serial port.
     */
    void applyTermios()
    {
        _termios.apply(_fd,getName());
    }

    /**
     * open the serial port. The current Termios settings
     * are also applied to the port.
     */
    void open(int flags = O_RDONLY) throw(nidas::util::IOException);

    /**
     * close the file descriptor.
     */
    void close() throw(nidas::util::IOException);

    int getFd() const { return _fd; }

    /* 
     * Check whether this serial port is using a device which needs port control
     */
    void checkPortControlRequired(const std::string& name);

    /**
     *  Get the SerialPortPhysicalControl object for direct updating
     */
    SerialPortPhysicalControl* getPortControl() {return _pSerialControl;}

    /**
     *  Set and retrieve the _portType member attribute 
     */
    void setPortType( const PORT_TYPES thePortType) {_portType = thePortType;}
    PORT_TYPES getPortType() {return _portType;}

    /**
     *  Commands the serial board to set the GPIO switches to configure for 
     *  the port type specified in the _portType member attribute.
     */
    void applyPortType();

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
    void setRTS485(int val);

    /**
     * Get the current state of the modem bits.
     * Do "man tty_ioctl" from Linux for more information.
     * These macros are useful for checking/setting the value of
     * of individual bits:
     * @code
     TIOCM_LE        DSR (data set ready/line enable)
     TIOCM_DTR       DTR (data terminal ready)
     TIOCM_RTS       RTS (request to send)
     TIOCM_ST        Secondary TXD (transmit)
     TIOCM_SR        Secondary RXD (receive)
     TIOCM_CTS       CTS (clear to send)
     TIOCM_CAR       DCD (data carrier detect)
     TIOCM_CD         see TIOCM_CAR
     TIOCM_RNG       RNG (ring)
     TIOCM_RI         see TIOCM_RNG
     TIOCM_DSR       DSR (data set ready)
     * @endcode
     */
    int getModemStatus();

    /**
     * Set the current state of the modem bits.
     */
    void setModemStatus(int val);

    /**
     * Clear the indicated modem bits.
     */
    void clearModemBits(int val);

    /**
     * Set the indicated modem bits.
     */
    void setModemBits(int val);

    bool getCarrierDetect();

    static std::string modemFlagsToString(int modem);

    void setBlocking(bool val);
    bool getBlocking();

    /**
     * Do a tcdrain() system call on the device. According to the tcdrain man page, it
     * "waits until all output written to the object referred to by fd has been transmitted".
     */
    void drain();

    /**
     * Do a tcflush() system call on the device. According to the tcflush man page, it
     * "discards data received but not read".
     */
    void flushInput();

    /**
     * Do a tcflush() system call on the device. According to the tcflush man page, it
     * "discards data written to the object referred to by fd but not transmitted".
     */
    void flushOutput();

    void flushBoth();

    int timeoutOrEOF() const { return _state == TIMEOUT_OR_EOF; }

    /**
     * Read bytes until either the term character is read, or len-1 number
     * of characters have been read. buf will be null terminated.
     *
     */
    virtual int readUntil(char *buf,int len,char term);

    /**
     * Do a readUntil with a newline terminator.
     */
    virtual int readLine(char *buf,int len);

    virtual int read(char *buf,int len) throw(nidas::util::IOException);

    virtual char readchar();

    virtual int write(const void *buf,int len) throw(nidas::util::IOException);

    /**
     * Static utility that creates a pseudo-terminal, returning the
     * file descriptor of the master side and creating a symbolic
     * link with the given name to the slave side.
     * @param linkname: Name of symbolic link to be created that links to the
     *	slave side of the pseudo-terminal. If a symbolic link already exists
     *	with that name it will be removed and re-created. If linkname already
     *	exists and it isn't a symbolic link, an error will be returned.
     * @return The file descriptor of the master side of the pseudo-terminal.
     *
     * Note: the symbolic link should be deleted when the file descriptor to
     * the master pseudo-terminal is closed. Otherwise, because of the way
     * the system recycles pseudo-terminal devices, the link may at some
     * time point to a different pseudo-terminal, probably created by a
     * different process, like sshd. Opening and reading/writing to the symbolic
     * link would then effect the other process, if the open was permitted.
     */
    static int createPtyLink(const std::string& linkname);

protected:

    nidas::util::Termios _termios;

    int _rts485;

    unsigned int _usecsperbyte;

    PORT_TYPES _portType;

    TERM _term;

    SerialPortPhysicalControl* _pSerialControl;

    /**
     * No assignment.
     */
    SerialPortIODevice& operator=(const SerialPortIODevice&);

    enum state { OK, TIMEOUT_OR_EOF} _state;

    char *_savep;

    char *_savebuf;

    int _savelen;

    int _savealloc;

    bool _blocking;
};

}}	// namespace nidas namespace core

#endif
