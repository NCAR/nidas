// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2022, Copyright University Corporation for Atmospheric Research
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

#include "Uio48Sensor.h"

#include <nidas/core/Looper.h>
#include <nidas/core/Variable.h>
#include <nidas/util/Logger.h>

#include <iomanip>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

NIDAS_CREATOR_FUNCTION(Uio48Sensor)

namespace n_u = nidas::util;

#ifdef HAVE_UIO48_H
Uio48::Uio48(int npins) : _devName(), _fd(-1), _npins(npins)
{
}

Uio48::~Uio48()
{
    close();
}

void Uio48::open(const std::string& device)
{
    _devName = device;
    if ((_fd = ::open(device.c_str(), O_RDWR)) < 0)
        throw n_u::IOException(device, "open", errno);
    getPins();
}

void Uio48::close()
{
    if (_fd >= 0) ::close(_fd);
    _fd = -1;
}

/*
 * ioctls defined in uio48.h:
 *  IOCTL_READ_PORT  port: 0-5, reads a byte
 *  IOCTL_WRITE_PORT  port: 0-5, writes a byte
 *  IOCTL_READ_BIT   bit numbered 1-48
 *  IOCTL_WRITE_BIT, but numbered 1-48
 *  IOCTL_SET_BIT   bit numbered 1-48
 *  IOCTL_CLR_BIT
 *  IOCTL_LOCK_PORT  port: 0-5
 *  IOCTL_UNLOCK_PORT
 */

void Uio48::clearPins(const nidas::util::BitArray& which)
{
    for (int bit = 0; bit < which.getLength(); bit++) {
	if (which.getBit(bit)) {
	    /*
	     * Note the inverted logic. Writing 1 to a bit position causes
	     * the corresponding output pin to sink current (up to 12 mA),
	     * effectively pulling it low.
	     */
	    unsigned long ival = 1;
	    ival |= (bit + 1) << 8;
	    int ret = ::ioctl(_fd, IOCTL_WRITE_BIT, ival);
	    if (ret < 0)
		throw n_u::IOException(getName(), "IOCTL_WRITE_BIT", errno);
	}
    }
}

void Uio48::setPins(const nidas::util::BitArray& which)
{
    for (int bit = 0; bit < which.getLength(); bit++) {
	if (which.getBit(bit)) {
	    /*
	     * Note the inverted logic. Writing 0 to a bit position causes
	     * the corresponding output pin to go to a high impedance state,
	     * which allows it to be used as an input.
	     */
	    unsigned long ival = 0;
	    ival |= (bit + 1) << 8;
	    int ret = ::ioctl(_fd, IOCTL_WRITE_BIT, ival);
	    if (ret < 0)
		throw n_u::IOException(getName(), "IOCTL_WRITE_BIT", errno);
	}
    }
}

void Uio48::setPins(const nidas::util::BitArray& which,
                    const nidas::util::BitArray& val)
{
    for (int bit = 0; bit < which.getLength(); bit++) {
	if (which.getBit(bit)) {
	    /* Again note the inverted logic. */
	    unsigned long ival = !val.getBit(bit);
	    ival |= (bit + 1) << 8;
	    int ret = ::ioctl(_fd, IOCTL_WRITE_BIT, ival);
	    if (ret < 0)
		throw n_u::IOException(getName(), "IOCTL_WRITE_BIT", errno);
	}
    }
}

nidas::util::BitArray Uio48::getPins()
{
    nidas::util::BitArray pins(_npins);

    // both IOCTL_READ_PORT and IOCTL_READ_BIT work. We'll use READ_PORT
#define USE_READ_PORT
#ifdef USE_READ_PORT
    for (int port = 0; port < 6; port++) {
        int ret = ::ioctl(_fd, IOCTL_READ_PORT, (unsigned long) port);
	if (ret < 0)
	    throw n_u::IOException(getName(), "IOCTL_READ_PORT", errno);

	ret = (ret ^ 0xff) & 0xff;	// flip bits
        pins.setBits(port*8, (port+1)*8, (unsigned) ret);
    }
#else
    for (int bit = 0; bit < _npins; bit++) {
        int ret = ::ioctl(_fd, IOCTL_READ_BIT, (unsigned long) (bit + 1));
	if (ret < 0)
	    throw n_u::IOException(getName(), "IOCTL_READ_BIT", errno);
	ret ^= 1;
        pins.setBit(bit, ret);
    }
#endif
    return pins;
}
#endif

Uio48Sensor::Uio48Sensor(): _nvars(0), _stag(0)
#ifdef HAVE_UIO48_H
    , _uio48(), _pipefds{-1, -1},
    _iodevice(0), _looperClient(*this, _uio48, _pipefds[1])
#endif
{
}

Uio48Sensor::~Uio48Sensor()
{
}

void Uio48Sensor::init()
{
}

void Uio48Sensor::validate()
{
    DSMSensor::validate();

    if (getSampleTags().size() != 1)
        throw n_u::InvalidParameterException(getName(), "sample",
            "must have exactly one sample");
    _stag = *getSampleTags().begin();
    _nvars = _stag->getVariables().size();

    const vector<Variable*>& vars = _stag->getVariables();
    vector<Variable*>::const_iterator iv = vars.begin();

    for ( ; iv != vars.end(); ++iv) {
        if ((*iv)->getLength() > 1)
            throw n_u::InvalidParameterException(getName(), "variable",
                "cannot have length > 1");
    }
}

bool Uio48Sensor::process(const Sample* samp,std::list<const Sample*>& results) throw()
{
    assert(samp->getType() == CHAR_ST);

    int slen = samp->getDataLength();

    const unsigned char* in = (const unsigned char*)
        samp->getConstVoidDataPtr();

    int nbin = *in++;

    int nv = std::min( std::min(nbin / 3, (slen-1) / 3), _nvars);

    SampleT<float>* outsamp = getSample<float>(_nvars);
    outsamp->setTimeTag(samp->getTimeTag() - getLagUsecs());
    outsamp->setId(_stag->getId());

    float* dp = outsamp->getDataPtr();

    for (int i = 0; i < nv; i++, in+=3) {
        unsigned int pins = in[0] + (in[1] << 8) + (in[2] << 16);
        *dp++ = (float) pins;
    }

    for (int i = nv; i < _nvars; i++)
        *dp++ = floatNAN;

    results.push_back(outsamp);

    return true;
}

IODevice* Uio48Sensor::buildIODevice()
{
#ifndef HAVE_UIO48_H
    return 0;
#else
    if (!_iodevice)
        _iodevice = new MyIODevice();   // deleted in DSMSensor dtor
    return _iodevice;
#endif
}

SampleScanner* Uio48Sensor::buildSampleScanner()
{
#ifndef HAVE_UIO48_H
    return 0;
#else
    SampleScanner* mscanr = new MessageStreamScanner();

    // The process() method converts a bit array of digital I/O pins to a float.
    // Each single precision float has 24 bit mantissa, so each
    // output variable can contain 24 pins of information.
    //
    // Format of data written on the pipe is a simple buffer of bytes. The first
    // byte is the length in bytes of the following sample, followed by bytes,
    // each containing 8 digio pins.
    //
    // So if the user has configured 1 variable, then the first 24 digital I/O
    // pins are queried, and the raw sample will contain:
    //  byte 0  length = 3
    //  byte 1  value of digital I/O pins 0-7
    //  byte 2  value of digital I/O pins 8-15
    //  byte 3  value of digital I/O pins 16-23

    int nbits = _nvars * 24;
    unsigned char len = (unsigned char) (nbits / 8);  // number of bytes in raw message
    string sepstr((const char*) &len, 1);
    mscanr->setMessageParameters(_nvars * 3, sepstr, false);
    return mscanr;
#endif
}

#ifdef HAVE_UIO48_H
void Uio48Sensor::open(int flags)
#else
void Uio48Sensor::open(int)
#endif
{
#ifndef HAVE_UIO48_H
    throw n_u::IOException(getDeviceName(), "open", "built without uio48-dev");
#else

    if (_uio48.getFd() < 0) _uio48.open(getDeviceName());

    if (::pipe(_pipefds) < 0)
        throw n_u::IOException(getDeviceName(), "pipe", errno);

    Looper* looper = getLooper();

    _looperClient.setFd(_pipefds[1]);

    looper->addClient(&_looperClient, MSECS_PER_SEC, 0);

    DSMSensor::open(flags);
#endif
}

#ifdef HAVE_UIO48_H
void Uio48Sensor::close()
{
    getLooper()->removeClient(&_looperClient);

    if (_pipefds[0] >= 0) ::close(_pipefds[0]);
    if (_pipefds[1] >= 0) ::close(_pipefds[1]);
    _pipefds[0] = _pipefds[1] = -1;

    // _iodevice->close() will do nothing if its fd is < 0
    _iodevice->setFd(-1);

    DSMSensor::close();
}

Uio48Sensor::MyLooperClient::MyLooperClient(const DSMSensor& sensor,
        Uio48& uio, int pipefd) :
    _sensor(sensor), _uio(uio), _pipefd(pipefd), _buffer()
{
    unsigned char* buf = new unsigned char[_uio.getNumPins() / 8 + 1];
    buf[0] = (unsigned char) _uio.getNumPins() / 8;
    _buffer.reset(buf);
}
void Uio48Sensor::MyLooperClient::looperNotify()
{
    // read pins
    n_u::BitArray bits = _uio.getPins();

    // store in buffer
    for (int i = 0; i < bits.getLengthInBytes(); i++)
        _buffer.get()[i+1] = bits.getBits(i*8, (i+1) * 8);

    // write to pipe
    if (::write(_pipefd, _buffer.get(), _uio.getNumPins() / 8 + 1) < 0) {
        n_u::IOException e(_sensor.getName() + " pipe", "write", errno);
        PLOG(("%s", e.what()));
    }
}
#endif
