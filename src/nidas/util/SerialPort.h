// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

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

#include <nidas/util/Termios.h>
#include <nidas/util/IOException.h>

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

    SerialPort(const std::string& name);

    /**
     * For serial port that is already open (stdin for example).
     * */
    SerialPort(const std::string& name, int fd) throw (IOException);

    /**
     * close the file descriptor if is is open.
     */
    virtual ~SerialPort();

    /**
     * Writable reference to the SerialPort's Termios.
     * If the SerialPort is open, call applyTermios()
     * after making modifications.
     */
    Termios& termios() { return _termios; }
    
    const Termios& getTermios() const { return _termios; }

    /**
     * Apply the Termios settings to an opened serial port.
     */
    void applyTermios() throw(IOException);

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
    virtual int open(int mode = O_RDONLY) throw (IOException);

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
    int getModemStatus() throw (IOException);

    /**
     * Set the current state of the modem bits.
     */
    void setModemStatus(int val) throw (IOException);

    /**
     * Clear the indicated modem bits.
     */
    void clearModemBits(int val) throw (IOException);

    /**
     * Set the indicated modem bits.
     */
    void setModemBits(int val) throw (IOException);

    bool getCarrierDetect() throw (IOException);

    static std::string modemFlagsToString(int modem);

    void setBlocking(bool val) throw(IOException);
    bool getBlocking() throw(IOException);

    /**
     * Do a tcdrain() system call on the device. According to the tcdrain man page, it
     * "waits until all output written to the object referred to by fd has been transmitted".
     */
    void drain() throw(IOException);

    /**
     * Do a tcflush() system call on the device. According to the tcflush man page, it
     * "discards data received but not read".
     */
    void flushInput() throw(IOException);

    /**
     * Do a tcflush() system call on the device. According to the tcflush man page, it
     * "discards data written to the object referred to by fd but not transmitted".
     */
    void flushOutput() throw(IOException);

    void flushBoth() throw(IOException);

    int timeoutOrEOF() const { return _state == TIMEOUT_OR_EOF; }

    /**
     * Read bytes until either the term character is read, or len-1 number
     * of characters have been read. buf will be null terminated.
     *
     */
    virtual int readUntil(char *buf,int len,char term) throw(IOException);

    /**
     * Do a readUntil with a newline terminator.
     */
    virtual int readLine(char *buf,int len) throw(IOException);

    virtual int read(char *buf,int len) throw(IOException);

    virtual char readchar() throw(IOException);

    virtual int write(const void *buf,int len) throw(IOException);

    /**
     * Static utility that creates a pseudo-terminal, returning the
     * file descriptor of the master side and creating a symbolic
     * link with the given name to the slave side.
     * @param linkname: Name of symbolic link to be created that links to the
     *	slave side of the pseudo-terminal. If a symbolic link already exists
     *	with that name it will be removed and re-created. If linkname already
     *	exists and it isn't a symbolic link, an error will be returned.
     * @return The file descriptor of the master side of the pseudo-terminal.
     */
    static int createPtyLink(const std::string& linkname) throw(IOException);

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
