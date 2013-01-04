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

#ifndef NIDAS_UTIL_TERMIOS_H
#define NIDAS_UTIL_TERMIOS_H

/*
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
*/
#include <termios.h>
#include <sys/ioctl.h>

#include <nidas/util/IOException.h>

#include <string>

namespace nidas { namespace util {

/**
 * A class providing get/set methods into a termios structure.
 */
class Termios {

public:

    /**
     * Default constructor: 9600 n81, no flow control, canonical input, output.
     */
    Termios();

    /**
     * Construct from an existing struct termios.
     */
    Termios(const struct termios*);

    /**
     * Construct from an opened serial port.
     */
    Termios(int fd,const std::string& devname)
        throw(IOException);

    virtual ~Termios() {}

    /**
     * Set the termios options on a serial port.
     */
    void apply(int fd,const std::string& devname)
        throw(IOException);

    /**
     * Set all Termios parameters from the contents of struct termios.
     */
    void set(const struct termios*);

    /**
     * Get a const pointer to the internal struct termios.
     */
    const struct termios* get();

    bool setBaudRate(int val);
    int getBaudRate() const;

    enum parity { NONE, ODD, EVEN};

    void setParity(enum parity val);
    parity getParity() const;
    std::string getParityString() const;

    /**
     * Set number of data bits to 5,6,7 or 8.
     */
    void setDataBits(int val);
    int getDataBits() const;

    /**
     * Set number of stop bits, to 1 or 2.
     */
    void setStopBits(int val);
    int getStopBits() const;

    /**
     * If local, then ignore carrier detect modem control line.
     */
    void setLocal(bool val);
    bool getLocal() const;

    /**
     * HARDWARE flow control is CTSRTS. SOFTWARE is Xon/Xoff.
     */
    enum flowcontrol { NOFLOWCONTROL, HARDWARE, SOFTWARE };
    typedef enum flowcontrol flowcontrol;

    /**
     * Set flow control to NOFLOWCONTROL, HARDWARE or SOFTWARE.
     */
    void setFlowControl(flowcontrol val);
    flowcontrol getFlowControl() const;
    std::string getFlowControlString() const;

    /**
     * Sets termios options for raw or non-raw(cooked) mode.
     * @param val false sets the serial port for "cooked" mode, where
     *      canonical processing is applied to input lines, and output
     *      lines are post-processed before transmission.  Input carriage
     *      returns are converted to newlines, and output newlines are 
     *      converted to carriage returns. For other conversions, 
     *      the appropriate bits (IGNCR, INLCR, ICRNL, ONLCR, OCRNL)
     *      from termios.h should be set in iflag() and oflag() after
     *      the call to setRaw(). true sets the serial port for "raw" mode, where
     *      canonical input and post output processing are not performed.
     *      THE VMIN and VTIME members of c_cc are set to getRawLength()
     *      and getRawTimeout() respectfully.
     */
    void setRaw(bool val);
    bool getRaw() const;

    void setRawLength(unsigned char val);
    unsigned char getRawLength() const;

    void setRawTimeout(unsigned char val);
    unsigned char getRawTimeout() const;

    tcflag_t &iflag() { return _tio.c_iflag; }
    tcflag_t &oflag() { return _tio.c_oflag; }
    tcflag_t &cflag() { return _tio.c_cflag; }
    tcflag_t &lflag() { return _tio.c_lflag; }
    cc_t *cc() { return _tio.c_cc; }

    tcflag_t getIflag() const { return _tio.c_iflag; }
    tcflag_t getOflag() const { return _tio.c_oflag; }

    static struct baudtable {
        unsigned int cbaud;
        int rate;
    } bauds[];

    void setDefaultTermios();

private:

    struct termios _tio;

    unsigned char _rawlen;

    unsigned char _rawtimeout;
};

}}	// namespace nidas namespace util

#endif
