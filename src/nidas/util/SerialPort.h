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

#ifndef NIDAS_UTIL_SERIALPORT_H
#define NIDAS_UTIL_SERIALPORT_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <iostream>

#include "Termios.h"
#include "IOException.h"

namespace nidas { namespace util {

class SerialPort
{

public:

    SerialPort();

    /**
     * Copy constructor.  The attributes of the port are copied,
     * but if the original is opened, the copy will not be
     * opened.
     */
    SerialPort(const SerialPort&);

    /**
     * Constructor, given a device name. The device is *NOT* opened, mainly
     * just to avoid throwing an exception in the constructor. Perhaps
     * that should be changed.
     */
    SerialPort(const std::string& name);

    /**
     * For serial port that is already open (stdin for example).
     * */
    SerialPort(const std::string& name, int fd);

    /**
     * close the file descriptor if is is open.
     */
    virtual ~SerialPort();

    /**
     * Writable reference to the SerialPort's Termios.
     * If the SerialPort is open, the user should call
     * applyTermios() for any modifications to take effect.
     */
    Termios& termios() { return _termios; }
    
    /**
     * Readonly reference to Termios.
     */
    const Termios& getTermios() const { return _termios; }

    /**
     * Apply the Termios settings to an opened serial port.
     */
    void applyTermios();

    /**
     * Get device name of the SerialPort.
     */
    const std::string& getName() const { return _name; }

    /**
     * Set device name of the SerialPort.
     */
    void setName(const std::string& val) { _name = val; }

    /**
     * open the serial port. The current Termios settings
     * are also applied to the port.
     */
    virtual int open(int mode = O_RDONLY);

    /**
     * close the file descriptor.
     */
    void close();

    // int &fd() { return _fd; }

    int getFd() const { return _fd; }

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

    virtual int read(char *buf,int len);

    virtual char readchar();

    virtual int write(const void *buf,int len);

    /**
     * Static utility that creates a pseudo-terminal, returning the
     * file descriptor of the master side and creating a symbolic
     * link with the given name to the slave side.  It is a convenient
     * wrapper for this code:
     *
     * @code
     * int fd = createPty(false);
     * createLinkToPty(linkname, fd);
     * @endcode
     *
     * @param linkname: Name of symbolic link to be created that links to the
     * slave side of the pseudo-terminal. If a symbolic link already exists
     * with that name it will be removed and re-created. If linkname already
     * exists and it isn't a symbolic link, an error will be returned.

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

    /**
     * Create a symbolic link to ptsname(fd).  @p fd must be a pty, such as
     * returned by createPty().
     */
    static void createLinkToPty(const std::string& linkname, int fd);

    /**
     * @brief Create a pseudo-terminal and return the file descriptor.
     * 
     * This is called by createPtyLink(), but it can also be called separately
     * if the caller wants to pass @p hup as true to set the HUP condition on
     * the slave side before creating the link.
     */
    static int createPty(bool hup);

    /**
     * Wait up to @p timeout seconds for pts to be opened.
     * 
     * Wait up to the @p timeout for the HUP to be cleared.  If the HUP
     * condition was set when the pty was created, then the cleared HUP means
     * the ptsname(fd) device has been opened.  Wait forever if @p timeout is
     * negative.  The poll() does not block on the fd, so it is called at
     * 1-second intervals using sleep(), until the timeout expires or the poll
     * returns with the POLLHUP bit cleared.
     * 
     * So here is a typical usage:
     * 
     * @code
     * int fd = createPty(true);
     * createLinkToPty(link, fd);
     * bool opened = waitForOpen(fd, -1);
     * @endcode
     * 
     * The existence of the link is a synchronization point, indicating the
     * pty has been created and HUP set, so it's safe to wait for HUP to be
     * cleared to know that it has been opened.
     */
    static bool waitForOpen(int fd, int timeout);

private:

    /**
     * No assignment.
     */
    SerialPort& operator=(const SerialPort&);

    Termios _termios;

    int _fd;

    std::string _name;

    enum state { OK, TIMEOUT_OR_EOF} _state;

    char *_savep;

    char *_savebuf;

    int _savelen;

    int _savealloc;

    bool _blocking;
};

}}	// namespace nidas namespace util

#endif
