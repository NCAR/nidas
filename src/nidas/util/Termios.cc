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

#ifdef GPP_2_95_2
#include <strstream>
#else
#include <sstream>
#endif

#include <nidas/util/Termios.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>

using namespace std;
using namespace nidas::util;

/* static */
Termios::baudtable Termios::bauds[] = {
    { B0,       0},
    { B50,      50},
    { B75,      75},
    { B110,     110},
    { B134,     134},
    { B150,     150},
    { B200,     200},
    { B300,     300},
    { B600,     600},
    { B1200,    1200},
    { B1800,    1800},
    { B2400,    2400},
    { B4800,    4800},
    { B9600,    9600},
    { B19200,   19200},
    { B38400,   38400},
    { B57600,   57600},
    { B115200,  115200},
    { B230400,  230400},
#ifdef B76800
    { B76800,   76800},
#endif
#ifdef B153600
    { B153600,  153600},
#endif
#ifdef B307200
    { B307200,  307200},
#endif
    { B460800,  460800},
    { B0,  -1}
};

Termios::Termios(): _tio(),_rawlen(0),_rawtimeout(0)
{
    setDefaultTermios();
}

Termios::Termios(const struct termios* termios_p): _tio(*termios_p),
    _rawlen(0),_rawtimeout(0)
{
    _tio = *termios_p;
    if (!(_tio.c_lflag & ICANON)) {
        _rawlen = _tio.c_cc[VMIN];
        _rawtimeout = _tio.c_cc[VTIME];
    }
}

Termios::Termios(int fd,const string& name) throw(IOException):
    _tio(),_rawlen(0),_rawtimeout(0)
{
    if (::tcgetattr(fd, &_tio) < 0)
        throw IOException(name ,"tcgetattr",errno);
    if (!(_tio.c_lflag & ICANON)) {
        _rawlen = _tio.c_cc[VMIN];
        _rawtimeout = _tio.c_cc[VTIME];
    }
}

void Termios::apply(int fd, const string& name) throw(IOException)
{
    if (::tcsetattr(fd, TCSANOW, &_tio) < 0)
        throw IOException(name,"tcsetattr",errno);
}

const struct termios* Termios::get()
{
    return &_tio;
}

void Termios::set(const struct termios* termios_p)
{
    _tio = *termios_p;
    if (!(_tio.c_lflag & ICANON)) {
        _rawlen = _tio.c_cc[VMIN];
        _rawtimeout = _tio.c_cc[VTIME];
    }
}

void Termios::setDefaultTermios()
{
    memset(&_tio,0,sizeof(_tio));
    _tio.c_iflag = IGNBRK | ICRNL;
    _tio.c_cflag = CS8 | CLOCAL | CREAD | B9600;
    _tio.c_oflag = OPOST | ONLCR;
    _tio.c_lflag = ICANON | ISIG | ECHOE | ECHOCTL | IEXTEN;
    _tio.c_cc[VINTR] = '\003';
    _tio.c_cc[VQUIT] = '\034';
    _tio.c_cc[VERASE] = '\177';
    _tio.c_cc[VKILL] = '\025';
    _tio.c_cc[VEOF] = '\004';
    _tio.c_cc[VEOL] = 0;
    cfsetispeed(&_tio,B9600);
    cfsetospeed(&_tio,B9600);
    _rawlen = 0;
    _rawtimeout = 0;
    // std::cerr << "cbaud=" << std::oct << (_tio.c_cflag & (CBAUD | CBAUDEX)) << std::dec << std::endl;
}

bool
Termios::setBaudRate(int val)
{

    int i;
    speed_t cbaud = B9600;
    for (i = 0; bauds[i].rate >= 0; i++)
        if (bauds[i].rate == val) {
            cbaud = bauds[i].cbaud;
            break;
        }
    if (bauds[i].rate < 0) return false;

    _tio.c_cflag &= ~(CBAUD | CBAUDEX);
    _tio.c_cflag |= cbaud;

    // std::cerr << "cbaud=" << std::oct << (_tio.c_cflag & (CBAUD | CBAUDEX)) << std::dec << std::endl;
    cfsetispeed(&_tio,cbaud);
    cfsetospeed(&_tio,cbaud);
    return true;
}

int Termios::getBaudRate() const
{ 
    speed_t cbaud = cfgetispeed(&_tio);
    cbaud = _tio.c_cflag & (CBAUD | CBAUDEX);
    int i;
    for (i = 0; bauds[i].rate >= 0; i++)
        if (bauds[i].cbaud == cbaud) return bauds[i].rate;
    return 0;
}

void Termios::setParity(enum parity val)
{
    switch (val) {
    case NONE:
        _tio.c_cflag &= ~PARENB;
        // disable parity checking
        _tio.c_iflag &= ~(INPCK);
        break;
    case EVEN:
        _tio.c_cflag |= PARENB;
        _tio.c_cflag &= ~PARODD;
        _tio.c_iflag |= INPCK;
        // don't ignore parity errors, but don't mark them either
        _tio.c_iflag &= ~(IGNPAR | PARMRK);
        break;
    case ODD:
        _tio.c_cflag |= PARENB;
        _tio.c_cflag |= PARODD;
        _tio.c_iflag |= INPCK;
        _tio.c_iflag &= ~(IGNPAR | PARMRK);
        break;
    }
}

Termios::parity
Termios::getParity() const
{
    if (!(_tio.c_cflag & PARENB)) return NONE;
    if (_tio.c_cflag & PARODD) return ODD;
    return EVEN;
}

void
Termios::setDataBits(int val)
{
    switch (val) {
    case 5: _tio.c_cflag = (_tio.c_cflag & ~CSIZE) | CS5; break;
    case 6: _tio.c_cflag = (_tio.c_cflag & ~CSIZE) | CS6; break;
    case 7:
            _tio.c_cflag = (_tio.c_cflag & ~CSIZE) | CS7;
            _tio.c_iflag |= ISTRIP;
            break;
    default:
    case 8:
            _tio.c_cflag = (_tio.c_cflag & ~CSIZE) | CS8;
            _tio.c_iflag &= ~ISTRIP;
            break;
    }
}

int
Termios::getDataBits() const
{
    int csize = _tio.c_cflag & CSIZE;
    switch(csize) {
    case CS5: return 5;
    case CS6: return 6;
    case CS7: return 7;
    default: return 8;
    }
}

void
Termios::setStopBits(int val)
{
    switch (val) {
    default:
    case 1: _tio.c_cflag &= ~CSTOPB; break;
    case 2: _tio.c_cflag |= CSTOPB; break;
    }
}

int Termios::getStopBits() const
{
    if (_tio.c_cflag & CSTOPB) return 2;
    return 1;
}

void
Termios::setLocal(bool val)
{
    if (val) {
        _tio.c_cflag |= CLOCAL;
        _tio.c_cflag &= ~HUPCL;
    }
    else {
        _tio.c_cflag &= ~CLOCAL;
        _tio.c_cflag |= HUPCL;
    }
}

bool Termios::getLocal() const
{
    return _tio.c_cflag & CLOCAL;
}

void
Termios::setFlowControl(flowcontrol val)
{
    switch (val) {
    case NOFLOWCONTROL:
        _tio.c_cflag &= ~CRTSCTS;
        _tio.c_iflag &= ~IXON;
        _tio.c_iflag &= ~IXOFF;
        _tio.c_iflag &= ~IXANY;
        break;
    case HARDWARE:
        _tio.c_cflag |= CRTSCTS;
        _tio.c_iflag &= ~IXON;
        _tio.c_iflag &= ~IXOFF;
        _tio.c_iflag &= ~IXANY;
        break;
    case SOFTWARE:
        _tio.c_cflag &= ~CRTSCTS;
        _tio.c_iflag |= IXON;
        _tio.c_iflag |= IXOFF;
        _tio.c_iflag |= IXANY;
        break;
    }
}

Termios::flowcontrol
Termios::getFlowControl() const
{
    if (_tio.c_cflag & CRTSCTS) return HARDWARE;
    if (_tio.c_iflag & IXON || _tio.c_iflag & IXOFF || _tio.c_iflag & IXANY)
        return SOFTWARE;
    return NOFLOWCONTROL;
}

void
Termios::setRaw(bool val)
{
    if (val) {
        _tio.c_iflag |= IGNBRK;
        /*
         * Watch out for meaning of IGNCR:  If the bit is set then
         * CR are tossed! It doesn't mean turn off translations!
         */
        _tio.c_iflag &= ~(IGNCR | INLCR | ICRNL | IUCLC );
        _tio.c_oflag &= ~OPOST;
        _tio.c_lflag &= ~(ICANON | ISIG | ECHO | ECHOE | IEXTEN | XCASE);
        _tio.c_cc[VMIN] = _rawlen;
        _tio.c_cc[VTIME] = _rawtimeout;
    }
    else {
        // cooked
        _tio.c_iflag |= IGNBRK | ICRNL;
        _tio.c_iflag &= ~(IGNCR | INLCR | IUCLC );
        _tio.c_oflag |= OPOST | ONLCR;
        _tio.c_lflag |= (ICANON | ISIG | ECHOE | ECHOCTL | IEXTEN);
        _tio.c_lflag &= ~ECHO;
        _tio.c_cc[VEOL] = 0x0;
        _tio.c_cc[VEOL2] = 0x0;
        _tio.c_cc[VEOF] = 0x04;
    }
}

bool
Termios::getRaw() const
{
    if (_tio.c_oflag & OPOST || _tio.c_lflag & ICANON) return false;
    return true;
}

void
Termios::setRawLength(unsigned char val)
{
    _rawlen = val;
    _tio.c_cc[VMIN] = _rawlen;
}

void
Termios::setRawTimeout(unsigned char val)
{
    _rawtimeout = val;
    _tio.c_cc[VTIME] = _rawtimeout;
}

unsigned char
Termios::getRawLength() const
{
    return _tio.c_cc[VMIN];
}

unsigned char
Termios::getRawTimeout() const
{
    return _tio.c_cc[VTIME];
}

std::string Termios::getParityString() const {
    switch(getParity()) {
    case NONE: return "none";
    case ODD: return "odd";
    case EVEN: return "even";
    }
    return "unknown";
}

std::string Termios::getFlowControlString() const {
    switch(getFlowControl()) {
    case NOFLOWCONTROL: return "none";
    case HARDWARE: return "hardware";
    case SOFTWARE: return "software";
    }
    return "unknown";
}

