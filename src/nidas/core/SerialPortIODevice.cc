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

#include "SerialPortIODevice.h"
#include "Looper.h"
#include "Prompt.h"
#include <nidas/core/HardwareInterface.h>
#include <nidas/util/Logger.h>
#include <nidas/util/time_constants.h>
#include <nidas/util/Exception.h>
#include <nidas/util/ptytools.h>
#include <nidas/util/util.h>

#include <cmath>
#include <sys/ioctl.h>
#include <sys/param.h>	// MAXPATHLEN
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::util;

namespace n_u = nidas::util;

namespace nidas { namespace core {

std::ostream& operator <<(std::ostream& rOutStrm, const PortConfig& rObj)
{
    rOutStrm << "Termios: baud: " << rObj.termios.getBaudRate()
         << " word: " << rObj.termios.getBitsString() << "; ";
    rOutStrm << "RTS485: " << rObj.rts485 << "; ";
    rOutStrm << "port_type: " << rObj.port_type << "; ";
    rOutStrm << "port_term: " << rObj.port_term;
    return rOutStrm;
}

SerialPortIODevice::SerialPortIODevice():
    UnixIODevice(), _workingPortConfig(),
    _usecsperbyte(0), _state(OK), _savep(0), _savebuf(0), _savelen(0), _savealloc(0), _blocking(false)
{
    _workingPortConfig.termios.setRaw(true);
    _workingPortConfig.termios.setRawLength(1);
    _workingPortConfig.termios.setRawTimeout(0);
}

SerialPortIODevice::SerialPortIODevice(const std::string& name, PortConfig initPortConfig):
    UnixIODevice(name), _workingPortConfig(initPortConfig),
    _usecsperbyte(0), _state(OK), _savep(0),_savebuf(0),_savelen(0),_savealloc(0),_blocking(false)
{
    _workingPortConfig.termios.setRaw(true);
    _workingPortConfig.termios.setRawLength(1);
    _workingPortConfig.termios.setRawTimeout(0);
}

SerialPortIODevice::~SerialPortIODevice()
{
    close();
    delete [] _savebuf;
}

void SerialPortIODevice::open(int flags) throw(n_u::IOException)
{
    VLOG(("SerialPortIODevice::open : entry"));

    UnixIODevice::open(flags);
    applyPortConfig();
    setBlocking(_blocking);

    // Not sure why rts485 had to be forced to -1 on ports that did not have
    // serial transceiver control.  Seems like this code should always do what
    // the config specifies.  For DSM3 ports that do have transceiver control,
    // then rts485 should not have any effect anyway.
#ifdef notdef
    // Set rts485 flag RS422/RS485 to always xmit for full RS422/485
    if (!_pXcvrCtrl) {
        if ( getPortType() == RS422) {
            DLOG(("RS422/485_FULL: forcing rts485 to -1, should get a high level on the line."));
            setRTS485(-1);
        }

        else {
#endif
            // set RTS according to how it's been set by the client regardless of the port type.
            // However, if the port type is RS485 half duplex, then the user needs to be sure of
            // how it needs to be set for the particular device.
            std::stringstream dStrm;
            dStrm << "Setting rts485 to as specified by client: " << getRTS485()
                      << ((getRTS485() < 0) ? ": should get a high level on the line." :
                          (getRTS485() > 0  ? ": should get a low level on the line." :
                          " RTS is \"do not care\""));
            DLOG((dStrm.str().c_str()));
            setRTS485(getRTS485());
#ifdef notdef
        }
    }
#endif

    VLOG(("SerialPortIODevice::open : exit"));
}

void SerialPortIODevice::applyPortConfig()
{
    applyTermios();

    HardwareDevice port(HardwareDevice::lookupDevice(getName()));
    if (auto iserial = port.iSerial())
    {
        PortConfig& pc = _workingPortConfig;
        DLOG(("") << "serial device " << getName() << ": setting "
                  << pc.port_type << ", " << pc.port_term);
        iserial->setConfig(pc.port_type, pc.port_term);
    }
    else
    {
        DLOG(("") << "serial device " << getName()
                  << ": no serial hardware control");
    }
}

int SerialPortIODevice::getUsecsPerByte() const
{
    int usecs = 0;
    if (::isatty(_fd)) {
        int bits = _workingPortConfig.termios.getDataBits() + _workingPortConfig.termios.getStopBits() + 1;
        if (_workingPortConfig.termios.getParity() != Parity::NONE)
        {
            bits++;
        }
        usecs = (bits * USECS_PER_SEC +_workingPortConfig.termios.getBaudRate() / 2) / _workingPortConfig.termios.getBaudRate();
    }
    return usecs;
}

void SerialPortIODevice::close() throw(n_u::IOException)
{
    if (_fd >= 0) {
        ::close(_fd);
        DLOG(("SerialPortIODevice::close(): ") << getName());
    }
    _fd = -1;
}

void SerialPortIODevice::setRTS485(int val)
{
    // If the remote device is RS485, and is set for half duplex, set RTS, which on many serial interfaces
    // results in a low output and shuts down the transmitter. This is usually necessary to be able to read
    // data from the remote device.  See the discussion about setRTS485() in the
    // header. 
    //
    //Ignore this if the current port is RS232 or LOOPBACK.

    // NOTE: if the value is 0, don't do anything. This is the default. This allows HW to do the heavy lifting 
    //       w/o getting the software involved.
    _workingPortConfig.rts485 = val;
	if (getRTS485() > 0) {
		// clear RTS
		clearModemBits(TIOCM_RTS);
	}
	else if (getRTS485() < 0) {
		// set RTS
		setModemBits(TIOCM_RTS);
	} // else ignore

	_usecsperbyte = getUsecsPerByte();
}


void SerialPortIODevice::setBlocking(bool val)
{
    if (_fd < 0) {
        _blocking = val;
        return;
    }
    int flags;
    if ((flags = fcntl(_fd,F_GETFL)) < 0)
        throw IOException(getName(),"fcntl F_GETFL",errno);

    if (val) flags &= ~O_NONBLOCK;
    else flags |= O_NONBLOCK;

    if (fcntl(_fd,F_SETFL,flags) < 0)
        throw IOException(getName(),"fcntl F_SETFL",errno);
    _blocking = val;
}

bool SerialPortIODevice::getBlocking()
{
    if (_fd < 0) return _blocking;

    int flags;
    if ((flags = fcntl(_fd,F_GETFL)) < 0)
        throw IOException(getName(),"fcntl F_GETFL",errno);

    _blocking = (flags & O_NONBLOCK) == 0;
    return _blocking;
}

int SerialPortIODevice::getModemStatus()
{
    int modem=0;

    if (!isapty(getName())) {
        if (::ioctl(_fd, TIOCMGET, &modem) < 0)
            throw IOException(getName(),"ioctl TIOCMGET",errno);
    }
    return modem;
}

void SerialPortIODevice::setModemStatus(int val)
{
    if (!isapty(getName())) {
        if (::ioctl(_fd, TIOCMSET, &val) < 0)
            throw IOException(getName(),"ioctl TIOCMSET",errno);
    }
}

void SerialPortIODevice::clearModemBits(int bits)
{
    if (!isapty(getName())) {
        if (::ioctl(_fd, TIOCMBIC, &bits) < 0)
            throw IOException(getName(),"ioctl TIOCMBIC",errno);
    }
}

void
SerialPortIODevice::setModemBits(int bits)
{
    if (!isapty(getName())) {
        if (::ioctl(_fd, TIOCMBIS, &bits) < 0)
            throw IOException(getName(),"ioctl TIOCMBIS",errno);
    }
}

bool SerialPortIODevice::getCarrierDetect()
{
    return (getModemStatus() & TIOCM_CAR) != 0;
}

string SerialPortIODevice::modemFlagsToString(int modem)
{
    string res;

#ifdef SHOW_ALL_ON_OFF
    static const char *offon[]={"OFF","ON"};
#endif

    static int status[] = {
        TIOCM_LE, TIOCM_DTR, TIOCM_RTS, TIOCM_ST, TIOCM_SR,
        TIOCM_CTS, TIOCM_CAR, TIOCM_RNG, TIOCM_DSR};
    static const char *lines[] =
    {"LE","DTR","RTS","ST","SR","CTS","CD","RNG","DSR"};

    for (unsigned int i = 0; i < sizeof status / sizeof(int); i++) {
#ifdef SHOW_ALL_ON_OFF
        res += lines[i];
        res += '=';
        res += offon[(modem & status[i]) != 0];
        res += ' ';
#else
        if (modem & status[i]) res += string(lines[i]) + ' ';
#endif
    }
    return res;
}

void SerialPortIODevice::drain()
{
    if (tcdrain(_fd) < 0)
        throw IOException(getName(),"tcdrain",errno);
}

void SerialPortIODevice::flushOutput()
{
    if (tcflush(_fd,TCOFLUSH) < 0)
        throw IOException(getName(),"tcflush TCOFLUSH",errno);
}

void SerialPortIODevice::flushInput()
{
    if (tcflush(_fd,TCIFLUSH) < 0)
        throw IOException(getName(),"tcflush TCIFLUSH",errno);
}

void SerialPortIODevice::flushBoth()
{
    if (tcflush(_fd,TCIOFLUSH) < 0)
        throw IOException(getName(),"tcflush TCIOFLUSH",errno);
}

int SerialPortIODevice::readUntil(char *buf, int len,char term)
{
    len--;		// allow for trailing null
    int toread = len;
    int rd,i,l;

    // check for data left from last read
    if (_savelen > 0) {

        l = toread < _savelen ? toread : _savelen;
        // #define DEBUG
#ifdef DEBUG
        cerr << "_savelen=" << _savelen << " l=" << l << endl;
#endif
        for (i = 0; i < l; i++) {
            toread--;_savelen--;
            if ((*buf++ = *_savep++) == term) break;
        }
        if (i < l) {	// term found
            *buf = '\0';
            return len - toread;
        }
#ifdef DEBUG
        cerr << "_savelen=" << _savelen << " l=" << l << " i=" << i << endl;
#endif
    }

    while (toread > 0) {
        switch(rd = read(buf,toread)) {
        case 0:		// EOD or timeout, user must figure out which
            *buf = '\0';
            return len - toread;
        default:
            for (; rd > 0;) {
                rd--;
                toread--;
#ifdef DEBUG
                cerr << "buf char=" << hex << (int)(unsigned char) *buf <<
                    " term=" << (int)(unsigned char) term << dec << endl;
#endif
                if (*buf++ == term) {
                    // save chars after term
                    if (rd > 0) {
                        if (rd > _savealloc) {
                            delete [] _savebuf;
                            _savebuf = new char[rd];
                            _savealloc = rd;
                        }
                        ::memcpy(_savebuf,buf,rd);
                        _savep = _savebuf;
                        _savelen = rd;
                    }
                    *buf = '\0';
                    return len - toread;
                }
            }
#ifdef DEBUG
            cerr << "rd=" << rd << " toread=" << toread << " _savelen=" << _savelen << endl;
#endif
            break;
        }
    }
    *buf = '\0';
    return len - toread;
}

int SerialPortIODevice::readLine(char *buf, int len)
{
    return readUntil(buf,len,'\n');
}

/**
 * Write to the device.
 */
std::size_t SerialPortIODevice::write(const void *buf, std::size_t len) throw(nidas::util::IOException)
{
    ssize_t result;

    // The notdef code was removed when xcvrConfig was replaced with
    // HardwareInterface.  The code limited rts485 to half-duplex port types
    // for ports which did not have hardware control.  That does not seem
    // useful to me (Gary).  A port which does not have hardware control is
    // not likely to have been assigned a port type, and old configs or
    // configs for ports which are not DSM3 serial card ports could still rely
    // on just rts485 to enable the transmitter.  So in this revision, just
    // always honor the rts485 config setting and do what it says.  It is up
    // to the config to be correct for the kind of port in use.  For DSM3
    // serial ports, the rts485 setting should have no effect, since the
    // transmitter is enabled separately from the RTS line, using the
    // connection from TXDEN on the FTDI to DIR1 on the SP339.
    //
    // There may not be any hardware left where nidas needs to control rts485,
    // so maybe all the rts485 handling could be removed someday soon.
#ifdef notdef
    // Current production FTDI board (the version that requires jumpers to set
    // the transceiver mode) does not directly manipulate RTS to control
    // transmission/reception on RS485 half duplex devices. And so the below
    // method was created to implement this control.

    // The current iteration of the FTDI board does control
    // transmission/reception on RS485 Half Duplex devices. However it is not
    // yet production ready (as of 02/27/2019).

    // The point being that soon the code needs to ascertain which regime
    // should be used, and then use that regime.
    if (getPortType() == PortType::RS485_HALF) {
        if (!SerialXcvrCtrl::xcvrCtrlSupported(getPortConfig().xcvrConfig.port)) {
            // remember that setting the FT4232H register has the opposite
            // effect on the RTS line signal which it outputs. Other UARTS may
            // behave differently.  YMMV.
            //
            // see the above discussion about RTS and 485. Here we try an
            // in-exact set/clear of RTS on either side of a write.
#endif
    if (getRTS485() != 0)
    {
        if (getRTS485() > 0) {
            // set RTS before write
            setModemBits(TIOCM_RTS);
        }
        else if (getRTS485() < 0) {
            // clear RTS before write
            clearModemBits(TIOCM_RTS);
        }
        VLOG(("Pre RS485 Half SerialPortIODevice::write() RTS state: ")
                << modemFlagsToString(getModemStatus() & TIOCM_RTS));
    }

    static LogContext lp(LOG_VERBOSE);
    if (lp.active())
    {
        auto data = std::string((char*)buf, ((char*)buf) + len);
        lp.log() << "SerialPortIODevice::write(): device: " << getName()
                 << " data: " << n_u::addBackslashSequences(data);
    }

    result = ::write(_fd, buf, len);
    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // let caller know that nothing was written and carry on...
            result = 0;
            DLOG(("SerialPortIODevice::write(): failed again/block "));
        }
        else {
            throw nidas::util::IOException(getName(),"write",errno);
        }
    }

#ifdef notdef
    if (getPortType() == RS485_HALF) {
        if (!SerialXcvrCtrl::xcvrCtrlSupported(getPortConfig().xcvrConfig.port)) {
            // Sleep until we think the last bit has been transmitted.
            // Add a fudge-factor of one quarter of a character.
            ::usleep(len * _usecsperbyte + _usecsperbyte/4);
            // set the RTS opposite of what was set before xmitting...
            if (getRTS485() > 0) {
                // then clear RTS
                clearModemBits(TIOCM_RTS);
            }
            else if (getRTS485() < 0) {
                // then set RTS
                setModemBits(TIOCM_RTS);
            }

            VLOG(("Post SerialPortIODevice::write() RTS state: ")
                    << modemFlagsToString(getModemStatus() & TIOCM_RTS));
        }
   }
#endif
    if (getRTS485() != 0)
    {
        ::usleep(len * _usecsperbyte + _usecsperbyte/4);
        // set the RTS opposite of what was set before xmitting...
        if (getRTS485() > 0) {
            // then clear RTS
            clearModemBits(TIOCM_RTS);
        }
        else if (getRTS485() < 0) {
            // then set RTS
            setModemBits(TIOCM_RTS);
        }
        VLOG(("Post SerialPortIODevice::write() RTS state: ")
                << modemFlagsToString(getModemStatus() & TIOCM_RTS));
    }
    return result;
}

int SerialPortIODevice::read(char *buf, int len, int timeout) throw(nidas::util::IOException)
{
    int charsRead = 0;
    if (timeout == 0) {
        DLOG(("SerialPortIODevice::read(): timeout is 0, read directly."));
        if ((charsRead = UnixIODevice::read(buf,len)) < 0)
        {
            ELOG(("SerialPortIODevice::read(): no timeout read fail."));
            throw IOException(getName(),"read",errno);
        }
    }
    else if ((charsRead = UnixIODevice::read(buf,len,timeout)) < 0) {
        ELOG(("SerialPortIODevice::read(): timeout > 0, read with poll fail."));
        throw IOException(getName(),"read",errno);
    }
    DLOG(("SerialPortIODevice::read(): read ") << charsRead << "chars.");
    // set the state for buffered read methods
    _state = (charsRead == 0) ? TIMEOUT_OR_EOF : OK;
#ifdef DEBUG
    cerr << "SerialPortIODevice::read len=" << len << endl;
#endif
    return charsRead;
}

/**
 * Do a buffered read and return character read.
 * If '\0' is read, then do a check of timeoutOrEOF()
 * to see if the basic read returned 0.
 */
char SerialPortIODevice::readchar()
{
    if (_savelen == 0) {
        if (_savealloc == 0) {
            delete [] _savebuf;
            _savealloc = 512;
            _savebuf = new char[_savealloc];
        }

        switch(_savelen = read(_savebuf,_savealloc)) {
        case 0:
            return '\0';
        default:
            _savep = _savebuf;
        }
    }
    _savelen--;
    return *_savep++;
}


}} // namespace nidas { namespace core
