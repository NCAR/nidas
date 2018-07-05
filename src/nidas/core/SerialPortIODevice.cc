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
#include <nidas/util/Logger.h>
#include <nidas/util/time_constants.h>
#include <nidas/util/Exception.h>

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
using namespace nidas::core;

SerialPortIODevice::SerialPortIODevice():
    UnixIODevice(),_termios(),_rts485(0),_usecsperbyte(0),
    _portType(RS232),_term(NO_TERM),_pSerialControl(0),_state(OK),
    _savep(0),_savebuf(0),_savelen(0),_savealloc(0),_blocking(true)
{
    _termios.setRaw(true);
    _termios.setRawLength(1);
    _termios.setRawTimeout(0);
}

SerialPortIODevice::SerialPortIODevice(const std::string& name, int fd):
    UnixIODevice(name),_termios(fd,name),_rts485(0),_usecsperbyte(0),
    _portType(RS232),_term(NO_TERM),_pSerialControl(0),_state(OK),
    _savep(0),_savebuf(0),_savelen(0),_savealloc(0),_blocking(true)
{
    getBlocking();
    checkPortControlRequired(getName());
}

SerialPortIODevice::SerialPortIODevice(const SerialPortIODevice& x):
    UnixIODevice(x.getName()),_termios(x._termios),_rts485(0),_usecsperbyte(0),
    _portType(RS232),_term(NO_TERM),_pSerialControl(const_cast<SerialPortIODevice&>(x).getPortControl()),_state(OK),
    _savep(0),_savebuf(0),_savelen(0),_savealloc(0),_blocking(x._blocking)
{
    checkPortControlRequired(getName());
}


SerialPortIODevice::SerialPortIODevice(const std::string& name, const PORT_TYPES portType, 
                                       const TERM term):
    UnixIODevice(name),_termios(),_rts485(0),_usecsperbyte(0),
    _portType(portType),_term(term),_pSerialControl(0),_state(OK),
    _savep(0),_savebuf(0),_savelen(0),_savealloc(0),_blocking(true)
{
    _termios.setRaw(true);
    _termios.setRawLength(1);
    _termios.setRawTimeout(0);

    checkPortControlRequired(name);
}

SerialPortIODevice::~SerialPortIODevice()
{
    close();
    delete [] _savebuf;
}

void SerialPortIODevice::checkPortControlRequired(const std::string& name)
{
    // if a port control object already exists, delete it first
    if (getPortControl()) {
        NLOG(("SerialPortIODevice::checkPortControlRequired(): _pSerialControl is not NULL..."));

        if (getName() != name) {
            NLOG(("SerialPortIODevice::checkPortControlRequired(): device names are different..."));
            delete _pSerialControl;
        }
    }

    NLOG(("SerialPortIODevice::checkPortControlRequired(): check if device is a DSM serial port device"));
    // Determine if this needs SP339 port type control
    std::string ttyBase = "/dev/ttyUSB";
    std::size_t foundAt = name.find(ttyBase);
    if (foundAt != std::string::npos) {
        NLOG(("SerialPortIODevice: Device needs SerialPortPhysicalControl object: ") << name);
        const char* nameStr = name.c_str();
        const char* portChar = &nameStr[ttyBase.length()];
        unsigned int portID = UINT32_MAX;
        istringstream portStream(portChar);

        try {
            portStream >> portID;
        }
        catch (exception e) {
            throw n_u::Exception("SerialPortIODevice: device name arg "
                                "cannot be parsed for canonical port ID");
        }

        NLOG(("SerialPortIODevice: Instantiating SerialPortPhysicalControl object on PORT") << portID 
            << "; Port type: " << _portType);
        _pSerialControl = new SerialPortPhysicalControl(static_cast<PORT_DEFS>(portID), 
                                                        _portType, _term);
        if (_pSerialControl == 0)
        {
            throw n_u::Exception("SerialPortIODevice: Cannot construct "
                                    "SerialPortPhysicalControl object");
        }
    }
}

void SerialPortIODevice::open(int flags) throw(n_u::IOException)
{
    UnixIODevice::open(flags);
    applyPortType();
    applyTermios();
    setBlocking(_blocking);

    // If the remote device is 485, clear RTS, which on many serial interfaces
    // shuts down the transmitter. This is usually necessary to be able to read
    // data from the remote device.  See the discussion about setRTS485() in the
    // header.
    if (_rts485) {
        int bits = TIOCM_RTS;
        if (_rts485 > 0) {
            // clear RTS
            if (::ioctl(_fd, TIOCMBIC, &bits) < 0)
                throw n_u::IOException(getName(),"ioctl TIOCMBIC",errno);
        }
        else {
            // set RTS
            if (::ioctl(_fd, TIOCMBIS, &bits) < 0)
                throw n_u::IOException(getName(),"ioctl TIOCMBIS",errno);
        }
        _usecsperbyte = getUsecsPerByte();
    }
}

int SerialPortIODevice::getUsecsPerByte() const
{
    int usecs = 0;
    if (::isatty(_fd)) {
        int bits = _termios.getDataBits() + _termios.getStopBits() + 1;
        switch(_termios.getParity()) {
        case n_u::Termios::ODD:
        case n_u::Termios::EVEN:
            bits++;
            break;
        case n_u::Termios::NONE:
            break;
        }
        usecs = (bits * USECS_PER_SEC + _termios.getBaudRate() / 2) / _termios.getBaudRate();
    }
    return usecs;
}

void SerialPortIODevice::applyPortType()
{
    if (_portType == RS232 || _portType == RS422 || _portType == RS485_FULL || _portType == RS485_HALF) {
        /**
         * TODO - is this an IOCTL operation?
         */
    }
    else {
        throw nidas::util::IOException(getName(),"_portType illegal value",_portType);
    }
}

void SerialPortIODevice::close() throw(n_u::IOException)
{
    if (_fd >= 0) {
        ::close(_fd);
        ILOG(("closing: ") << getName());
    }
    _fd = -1;
}

void SerialPortIODevice::setRTS485(int val)
{
    _rts485 = val;
    if (_rts485 && _fd >= 0) {
        int bits = TIOCM_RTS;
        if (_rts485 > 0) {
            // clear RTS
            if (::ioctl(_fd, TIOCMBIC, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIC",errno);
        }
        else {
            // set RTS
            if (::ioctl(_fd, TIOCMBIS, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIS",errno);
        }
        _usecsperbyte = getUsecsPerByte();
    }
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
    if (::ioctl(_fd, TIOCMGET, &modem) < 0)
        throw IOException(getName(),"ioctl TIOCMGET",errno);
    return modem;
}

void SerialPortIODevice::setModemStatus(int val)
{
    if (::ioctl(_fd, TIOCMSET, &val) < 0)
        throw IOException(getName(),"ioctl TIOCMSET",errno);
}

void SerialPortIODevice::clearModemBits(int bits)
{
    if (::ioctl(_fd, TIOCMBIC, &bits) < 0)
        throw IOException(getName(),"ioctl TIOCMBIC",errno);
}

void
SerialPortIODevice::setModemBits(int bits)
{
    if (::ioctl(_fd, TIOCMBIS, &bits) < 0)
        throw IOException(getName(),"ioctl TIOCMBIS",errno);
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

int SerialPortIODevice::write(const void *buf, int len) throw(nidas::util::IOException)
{
    if ((len = ::write(_fd,buf,len)) < 0) {
        if (!_blocking && errno == EAGAIN) return 0;
        throw IOException(getName(),"write",errno);
    }
    return len;
}

int SerialPortIODevice::read(char *buf, int len) throw(nidas::util::IOException)
{
    if ((len = ::read(_fd,buf,len)) < 0)
        throw IOException(getName(),"read",errno);
    // set the state for buffered read methods
    _state = (len == 0) ? TIMEOUT_OR_EOF : OK;
#ifdef DEBUG
    cerr << "SerialPortIODevice::read len=" << len << endl;
#endif
    return len;
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

/* static */
int SerialPortIODevice::createPtyLink(const std::string& link)
{
    int fd;
    const char* ptmx = "/dev/ptmx";

    // could also use getpt() here.
    if ((fd = ::open(ptmx,O_RDWR|O_NOCTTY)) < 0) 
        throw IOException(ptmx,"open",errno);

    char* slave = ptsname(fd);
    if (!slave) throw IOException(ptmx,"ptsname",errno);

    // cerr << "slave pty=" << slave << endl;

    if (grantpt(fd) < 0) throw IOException(ptmx,"grantpt",errno);
    if (unlockpt(fd) < 0) throw IOException(ptmx,"unlockpt",errno);

    bool dolink = true;
    struct stat linkstat;
    if (lstat(link.c_str(),&linkstat) < 0) {
        if (errno != ENOENT)
            throw IOException(link,"stat",errno);
    }
    else {
        if (S_ISLNK(linkstat.st_mode)) {
            char linkdest[MAXPATHLEN];
            int ld = readlink(link.c_str(),linkdest,MAXPATHLEN-1);
            if (ld < 0)
                throw IOException(link,"readlink",errno);
            linkdest[ld] = 0;
            if (strcmp(slave,linkdest)) {
                cerr << "Deleting " << link << " (a symbolic link to " << linkdest << ")" << endl;
                if (unlink(link.c_str()) < 0)
                    throw IOException(link,"unlink",errno);
            }
            else dolink = false;
        }
        else
            throw IOException(link,
                    "exists and is not a symbolic link","");

    }
    if (dolink) {
        cerr << "Linking " << slave << " to " << link << endl;
        if (symlink(slave,link.c_str()) < 0)
            throw IOException(link,"symlink",errno);
    }
    return fd;
}

