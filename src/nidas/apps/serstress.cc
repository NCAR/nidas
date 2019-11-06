/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
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
/*

    serstress: null modem style testing program for serial ports. Loops through the three port 
               types (RS232, RS422/485 Full Duplex, RS422/485 Half Duplex) and all the baud rates.
               At the end of each loop, it calculates transmission statistics. It sets up similar 
               to the 'serstress' utility.

*/

#include <nidas/util/BasicRunningStats.h>
#include <nidas/util/Exception.h>
#include <nidas/util/Logger.h>
#include <nidas/core/SerialPortIODevice.h>
#include <nidas/util/SerialOptions.h>
#include <nidas/util/SensorPowerCtrl.h>
#include <nidas/util/Thread.h>
#include <nidas/util/UTime.h>
#include <nidas/util/util.h>
#include <nidas/util/auto_ptr.h>
#include <nidas/util/SPoll.h>
#include <nidas/core/SerialXcvrCtrl.h>

#include <vector>
#include <cstring>
#include <ctime>
#include <memory>
#include <iostream>
#include <iomanip>
#include <assert.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <cstdlib>
#include <math.h>
#include <fenv.h>

using namespace std;
namespace n_u = nidas::util;
namespace n_c = nidas::core;

using nidas::util::LogScheme;
using nidas::util::Logger;
using nidas::util::LogConfig;

static const bool SENDING = true;
static const bool ECHOING = false;

static int baudStartIdx = 0;
static int baudTableSize = 1;

static float rate = -2.0;
static unsigned int debug = 0;
static bool ascii = false;
static int dataSize = 0;
static unsigned int nPacketsOut = 999998;
static string defaultTermioOpts = "n81lnr";
static string termioOpts = defaultTermioOpts;
static string device;
static string echoDevice;
static bool verbose = false;
static int timeoutSecs = -1;
static int interrupted = 0;
static unsigned int periodMsec = 0;

static n_c::PORT_TYPES portType = n_c::LOOPBACK;
static n_c::PORT_TYPES skipped_portType = n_c::RS485_HALF;
static n_c::SerialPortIODevice port;
static string shortName;
static n_c::SerialPortIODevice echoPort;
static string echoShortName;
static n_c::PortConfig portConfig;
static n_c::PortConfig echoPortConfig;

/**
 * Format of test packet:
 * @verbatim
 * NNNNNN MMMMMMMMM ccccc <data>HHHHHHHH\x04
 * NNNNNN: packet number starting at 0, 6 ASCII digits, followed by space.
 * MMMMMMMM: number of milliseconds since program start:
 *      9 digits followed by a space.
 * ccccc: length of data portion, in bytes, 0 or larger.
 *      5 digits, followed by space.
 * <data>: data portion of packet, may be 0 bytes in length.
 * HHHHHHHH: CRC of packet contents, up to but not including CRC (duh),
 *      8 ASCII hex digits.
 * \x04: trailing ETX byte
 * Length of packet is then 7 + 10 + 6 + dataSize + 8 + 1 = 32 + dataSize
 * @endverbatim
 */

const int START_OF_DATA = 23;   // data starts at byte 23
const int LENGTH_OF_CRC = 8;
const int MIN_PACKET_LENGTH = START_OF_DATA + LENGTH_OF_CRC + 1;
int MAX_PACKET_LENGTH = 65535;
int MAX_DATA_LENGTH = MAX_PACKET_LENGTH - MIN_PACKET_LENGTH;

char ETX = '\x04';
unsigned int EOF_NPACK = 999999;

class Sender: public n_u::Thread
{
public:
    Sender(bool ascii,int dsize);
    ~Sender();
    int run() throw(n_u::Exception);
    void send() throw(n_u::IOException);
    void flush() throw(n_u::IOException);
    unsigned int getNout() const { return _nout; }

    float getKbytePerSec() const
    {
        return (( _deltaT == 0) ? 0.0 : _byteSum / (float)_deltaT);
    }

    unsigned int getDiscarded() const
    {
        return _totalDiscarded;
    }

    bool getRS485HalfSend() const {return _rs485HalfSend;} 
    void setRS485HalfSend(bool send=true) {_rs485HalfSend = send;} 

private:
    bool _ascii;
    int _dsize;
    char* _dbuf;
    char* _buf;
    char* _eob;
    char* _hptr;
    char* _tptr;
    unsigned int _nout;

    time_t _sec0;
    int _msec0;

    int _packetLength;
    vector<int> _last100;
    vector<int> _msec100;
    int _msec100ago;

    unsigned int _byteSum;
    int _deltaT;

    unsigned int _discarded;
    unsigned int _totalDiscarded;
    bool _rs485HalfSend;

    Sender(const Sender&);
    Sender& operator=(const Sender&);
};

class Receiver: public n_u::Thread
{
public:
    Receiver(int timeoutSecs, Sender*);

    int run() throw(n_u::IOException);
    void miniReport();
    void activityIndicator();
    void accumulateBulkStats();
    void reportBulkStats();

    float getKbytePerSec() const
    {
        return (( _deltaT == 0) ? 0.0 : _byteSum / (float)_deltaT);
    }

private:

    void reallocateBuffer(int len);
    int scanBuffer();

    const int RBUFLEN;

    char* _buf;
    char* _rptr;
    char* _wptr;
    char* _eob;
    int _buflen;
    vector<int> _last10;
    vector<int> _last100;
    vector<int> _msec100;
    int _msec100ago;
    int _ngood10;
    int _ngood100;
    unsigned int _Npack;
    unsigned int _Nlast;
    int _dsize;
    int _dsizeTrusted;

    int _msec;
    bool _scanHeaderNext;

    time_t _sec0;
    int _msec0;

    unsigned int _byteSum;
    unsigned int _deltaT;

    int _roundTripMsecs;
    int _timeoutSecs;

    Sender* _sender;

    unsigned int _totalBad;
    n_u::BasicRunningStats _kbpsInStats;
    n_u::BasicRunningStats _kbpsOutStats;
    n_u::BasicRunningStats _roundTripMSecsStats;

    Receiver(const Receiver&);
    Receiver& operator=(const Receiver&);
};

/* cksum -- calculate and print POSIX checksums and sizes of files
   Copyright (C) 92, 1995-2006 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Q. Frank Xia, qx@math.columbia.edu.
   Cosmetic changes and reorganization by David MacKenzie, djm@gnu.ai.mit.edu.

  This software is compatible with neither the System V nor the BSD
  `sum' program.  It is supposed to conform to POSIX, except perhaps
  for foreign language support.  Any inconsistency with the standard
  (other than foreign language support) is a bug.  */


#include <stdio.h>
#include <stdint.h>

static uint32_t const crctab[256] =
{
  0x00000000,
  0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
  0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6,
  0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
  0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac,
  0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8, 0x6ed82b7f,
  0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a,
  0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58,
  0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033,
  0xa4ad16ea, 0xa06c0b5d, 0xd4326d90, 0xd0f37027, 0xddb056fe,
  0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
  0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4,
  0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
  0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5,
  0x2ac12072, 0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
  0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca, 0x7897ab07,
  0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c,
  0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1,
  0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b,
  0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698,
  0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d,
  0x94ea7b2a, 0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
  0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2, 0xc6bcf05f,
  0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
  0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80,
  0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
  0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a,
  0x58c1663d, 0x558240e4, 0x51435d53, 0x251d3b9e, 0x21dc2629,
  0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c,
  0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e,
  0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65,
  0xeba91bbc, 0xef68060b, 0xd727bbb6, 0xd3e6a601, 0xdea580d8,
  0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
  0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2,
  0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
  0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74,
  0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
  0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c, 0x7b827d21,
  0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a,
  0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e, 0x18197087,
  0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d,
  0x2056cd3a, 0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce,
  0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb,
  0xdbee767c, 0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
  0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4, 0x89b8fd09,
  0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
  0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf,
  0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static uint32_t
cksum (const unsigned char *input,size_t len)
{
  uint32_t crc = 0;
  uintmax_t length = len;

  while(len--)
	crc = (crc << 8) ^ crctab[((crc >> 24) ^ *input++) & 0xFF];

  for (; length; length >>= 8)
    crc = (crc << 8) ^ crctab[((crc >> 24) ^ length) & 0xFF];

  crc = ~crc & 0xFFFFFFFF;

  return crc;
}

Sender::Sender(bool a,int s):Thread("Sender"),_ascii(a),_dsize(s),
    _dbuf(0),_buf(0),_eob(0),_hptr(0),_tptr(0),_nout(0),
    _sec0(0),_msec0(0),_packetLength(0), _last100(),_msec100(),
    _msec100ago(0),_byteSum(0),_deltaT(0),_discarded(0), _totalDiscarded(0),
    _rs485HalfSend(true)
{
    _last100.resize(100);
    _msec100.resize(100);

    _packetLength = MIN_PACKET_LENGTH + _dsize;

    int outBuflen = 0;
    if (periodMsec == 0) outBuflen = 8192;
    outBuflen = std::max(outBuflen,_packetLength + 1);

    // cerr << "send buffer length=" << outBuflen << endl;

    _hptr = _tptr = _buf = new char[outBuflen];
    memset(_buf,0,outBuflen);   // avoids valgrind warnings
    _eob = _buf + outBuflen;

    if (_dsize > 0) _dbuf = new char[_dsize];

    struct timeval tv;
    gettimeofday(&tv,0);
    _sec0 = tv.tv_sec;
    _msec0 = (tv.tv_usec / USECS_PER_MSEC);

    if (_ascii) {
        for (int i = 0; i < _dsize; i++) {
            char c = ((i % 52) < 26) ? 'A' + (i % 26) : 'a' + (i % 26);
            _dbuf[i] = c;
        }
    }
    else {
        unsigned char seq[] = {0xaa,0x55,0xf0,0x0f,0x00,0xff};
        for (int i = 0; i < _dsize; i++) {
            char c = seq[i % sizeof(seq)];
            _dbuf[i] = c;
        }
    }
}

Sender::~Sender()
{
    delete [] _buf;
    delete [] _dbuf;
}

int Sender::run() throw(n_u::Exception)
{
    ILOG(("Starting Sender on port: ") << device);
    ILOG(("Current Sender modem status: ") << port.modemFlagsToString(port.getModemStatus()));

    for (;_nout < nPacketsOut;) { 
        if (isInterrupted() || interrupted) break;

        // Check to see if we need RS485 1/2 duplex signaling
        if (port.getPortType() == n_c::RS485_HALF) {
            int timeout = 10;
            while (!getRS485HalfSend() && timeout > 0) {
                n_u::sleepUntil(periodMsec);
                --timeout;
            }
            if (!timeout) {
                ELOG(("serstress::Sender.run(): Failed to hear back from Sender Receiver in RS485 Half Duplex..."));
                return RUN_EXCEPTION;
            }
            setRS485HalfSend(false);
        }
    
        send();
        if (periodMsec > 0) n_u::sleepUntil(periodMsec);
    }

    // write out last packet
    _nout = EOF_NPACK;
    send();
    flush();
    return RUN_OK;
}

void Sender::flush() throw(n_u::IOException)
{
    int len = _hptr - _tptr;
    if (len == 0) return;
    int l = port.write(_tptr,len);
    _tptr += l;
    if (_hptr == _tptr) 
        _hptr = _tptr = _buf;
}

void Sender::send() throw(n_u::IOException)
{
    struct timeval tv;
    gettimeofday(&tv,0);
    int msec = (int)(tv.tv_sec - _sec0) * MSECS_PER_SEC +
        (int)(tv.tv_usec / USECS_PER_MSEC - _msec0);

    int iout = _nout % 100;

    // We use sprintf to write into the buffer which adds a
    // trailing NULL (which isn't sent), so we need space for
    // one more byte.
    if (_hptr + _packetLength + 1 > _eob) {
        flush();
        ++_totalDiscarded;
        _byteSum -= _last100[iout];
        if (_msec100[iout] > 0)
            _msec100ago = _msec100[iout];
        _deltaT = msec - _msec100ago;
        _msec100[iout] = msec;
        _last100[iout] = 0;
        _nout++;
        return;  // can't write
    }
        
    int icrc = START_OF_DATA + _dsize;

    sprintf(_hptr,"%6u %9d %5u ",_nout,msec,_dsize);
    if (_dsize > 0) memcpy(_hptr + START_OF_DATA,_dbuf,_dsize);

    uint32_t crc = cksum((const unsigned char*)_hptr,icrc);
    sprintf(_hptr + icrc,"%08x%c",crc,ETX);
    if (verbose) cout << "sent " << _packetLength << ":" << 
            n_u::addBackslashSequences(string(_hptr,_packetLength)) << endl;

    _byteSum -= _last100[iout];
    if (_msec100[iout] > 0)
        _msec100ago = _msec100[iout];
    _deltaT = msec - _msec100ago;
    _byteSum += _packetLength;
    _msec100[iout] = msec;
    _last100[iout] = _packetLength;

    _hptr += _packetLength;

    // write out buffer...
    if (_hptr + 1 == _eob) flush();
    _nout++;
}

Receiver::Receiver(int timeoutSecs, Sender* s): Thread((s ? "SenderReceiver" : "EchoReceiver")),
    RBUFLEN(8192),_buf(0),_rptr(0),_wptr(0),_eob(0),_buflen(0),
    _last10(), _last100(), _msec100(),
    _msec100ago(0),_ngood10(0),_ngood100(0),
    _Npack(0),_Nlast(0),_dsize(0),_dsizeTrusted(0),
    _msec(0),_scanHeaderNext(true),_sec0(0),_msec0(0),
    _byteSum(0),_deltaT(0),_roundTripMsecs(0), _timeoutSecs(timeoutSecs),
    _sender(s), _totalBad(0), _kbpsInStats(), _kbpsOutStats(), _roundTripMSecsStats()
{
    _last10.resize(10);
    _last100.resize(100);
    _msec100.resize(100);
    reallocateBuffer(RBUFLEN);

    struct timeval tv;
    gettimeofday(&tv,0);
    _sec0 = tv.tv_sec;
    _msec0 = (tv.tv_usec / USECS_PER_MSEC);

}

void Receiver::reallocateBuffer(int len)
{
    // cerr << "reallocateBuffer, len=" << len << endl;
    delete [] _buf;
    char* newbuf = new char[len];
    memset(newbuf,0,len);   // avoids valgrind warnings
    int l = _wptr - _rptr;
    assert(l < len);
    if (l > 0) memcpy(newbuf,_rptr,l);
    _rptr = newbuf;
    _wptr = newbuf + l;
    _buf = newbuf;
    _eob = _buf + len;
    _buflen = len;
}

int Receiver::run() throw(n_u::IOException)
{
    // Am I the Sender Receiver or the Echo Receiver?
    n_c::SerialPortIODevice& myPort = _sender ? port : echoPort;
    string& myDevice = _sender ? device : echoDevice;

    SPoll poller(_timeoutSecs*MSECS_PER_SEC);
    poller.addPollee(myPort.getFd(), 0);

    ILOG(("Starting Receiver for ") << (_sender ? "Sender" : "Echo") << " on port: " << myPort.getName());
    ILOG(("Current ") << (_sender ? "Sender" : "Echo") << " modem status: " << myPort.modemFlagsToString(myPort.getModemStatus()));

    for ( ; isInterrupted() || !interrupted; ) {
        // wait for data
        int nfd = poller.poll();
        if (nfd < 0) {
            if (errno == EINTR) return RUN_EXCEPTION;
            throw n_u::IOException(myDevice,"select",errno);
        }
        if (nfd == 0) {
            cout << myDevice << ": timeout " << _timeoutSecs <<
                " seconds" << endl;
            return RUN_EXCEPTION;
        }
        ILOG(("Poll event on: ") << (_sender ? "Sender " : "Echo ") << "Receiver");
        int len = _eob - _wptr;
        assert(len > 0);
        int l = ::read(myPort.getFd(), _wptr, len);
        if (l == 0) {
            ILOG(("No data found on ") << (_sender ? "Sender " : "Echo ") << "Receiver");
            break;
        }
        ILOG(("Received data on: ") << (_sender ? "Sender " : "Echo ") << "Receiver");
        _wptr += l;
        if (scanBuffer()) break;
    }

    return RUN_OK;
}

/**
 * Return 0 on normal scan, 1 on receipt of EOF packet.
 */
int Receiver::scanBuffer()
{
    n_c::SerialPortIODevice& myPort = _sender ? port : echoPort;
    struct timeval tnow;

    int plen;

    for (;;) {
        bool goodPacket = false;
        if (_scanHeaderNext) {        // read header
            if (_wptr - _rptr < START_OF_DATA) {
                ILOG(("") << (_sender ? "Sender " : "Echo ") << "Receiver: not enuf data, wait for next read...");
                break;   // not enough data
            }
            if (sscanf(_rptr,"%u %d %d",&_Npack,&_msec,&_dsize) == 3 &&
                _dsize < MAX_DATA_LENGTH) {
#ifdef DEBUG
                cerr << "Npack=" << _Npack << " _msec=" << _msec << " _dsize=" << _dsize << endl;
#endif
                // if missing packets
                if (_Npack != EOF_NPACK) {
                    for (unsigned int n = _Nlast + 1; n < _Npack; n++) {
                        int iout = n % 10;
                        if (_ngood10 > 0 && _last10[iout] > 0) _ngood10--;
                        _last10[iout] = 0;
                        iout = n % 100;
                        if (_ngood100 > 0 && _last100[iout] > 0) _ngood100--;
                        _byteSum -= _last100[iout];
                        _last100[iout] = 0;
                    }
                    _Nlast = _Npack;
                }
                _scanHeaderNext = false;
                plen = MIN_PACKET_LENGTH + _dsize;
                // Note we haven't checked the CRC of this packet yet,
                // so perhaps this _dsize is bogus. We've tested that
                // it is less than MAX_DATA_LENGTH so it shouldn't cause
                // a crash. Before exiting scanBuffer we will reset the
                // buffer size if we've received a _dsize that passes
                // the CRC test.
                if (plen > _buflen) reallocateBuffer(plen);
            }
            else _dsize = 0;
        }
        if (!_scanHeaderNext) {
            plen = MIN_PACKET_LENGTH + _dsize;
            if (_wptr - _rptr < plen) break;

            gettimeofday(&tnow,0);
            int msec =
                (int)(tnow.tv_sec - _sec0) * MSECS_PER_SEC +
                (int)(tnow.tv_usec / USECS_PER_MSEC - _msec0);
            uint32_t crc = cksum((const unsigned char*)_rptr,START_OF_DATA + _dsize);
            uint32_t incrc = 0;
            if (_rptr[START_OF_DATA + _dsize + LENGTH_OF_CRC] == ETX &&
                sscanf(_rptr+START_OF_DATA+_dsize,"%8x",&incrc) == 1 &&
                incrc == crc) {
                // cerr << "good: Nlast=" << _Nlast << " _dsize=" << _dsize << endl;
                goodPacket = true;
                ILOG(("") << (_sender ? "Sender " : "Echo ") << "Receiver found good packet");
                if (verbose && _sender) cout << "rcvd " << plen << ":" <<
                    n_u::addBackslashSequences(string(_rptr,plen)) << endl;
                // cerr << "Nlast=" << _Nlast << " _last10[]=" <<
                  //   _last10[_Nlast % 10] << " ngood10=" << _ngood10 << endl;
                    
                int iout = _Nlast % 10;
                if (_last10[iout] == 0) _ngood10++;
                _last10[iout] = plen;

                iout = _Nlast % 100;
                if (_last100[iout] == 0) _ngood100++;
                _byteSum -= _last100[iout];
                _last100[iout] = plen;
                _byteSum += plen;
                if (_msec100[iout] > 0)
                    _msec100ago = _msec100[iout];
                _msec100[iout] = msec;
                _deltaT = msec - _msec100ago;
                _roundTripMsecs = msec - _msec;

                // TODO: buffer this depending on output rate
                if (!_sender) {
                    ILOG(("Echoing data to Sender Receiver..."));
                     myPort.write(_rptr,plen);     // echo back
                }
                _dsizeTrusted = _dsize;
                _rptr += plen;

            }
            _scanHeaderNext = true;
        }
        if (!goodPacket) {
            ILOG(("") << (_sender ? "Sender " : "Echo ") << "Receiver found bad packet");
            ++_totalBad;
            // bad packet, look for ETX to try to make some sense of this junk
            for ( ; _rptr < _wptr && *_rptr++ != ETX; );
        }

        else if (_sender) { // only the Send Receiver needs to do this.
            activityIndicator();
            accumulateBulkStats();
        }

        // If RS485_HALF port mode, then let the Sender thread send the next packet.
        if (_sender && (myPort.getPortType() == n_c::RS485_HALF)) {
            _sender->setRS485HalfSend();
        }

        if (_Npack == EOF_NPACK) {
            ILOG(("") << (_sender ? "Sender " : "Echo ") << "Receiver found last packet");
            return 1;
        }
    }
    unsigned int l = _wptr - _rptr;  // unscanned characters in buffer
    if (l > 0) memmove(_buf,_rptr,l);
    _rptr = _buf;
    _wptr = _buf + l;

    // Reset the buffer size to something reasonable if it
    // grew because of a bad dsize.
    if (_buflen > RBUFLEN) {
        int blen = std::max(MIN_PACKET_LENGTH + _dsizeTrusted,
            (int)(_wptr - _rptr));
        blen = std::max(blen,RBUFLEN);
        if (blen != _buflen) reallocateBuffer(blen);
    }
    return 0;
}

void Receiver::miniReport()
{
    if (_sender) {
        cout << shortName;
        cout << " sent#:" << setw(5) << _sender->getNout();
        cout << " rcvd#:" << setw(5) << _Nlast;
        cout << ", dT:" << setw(4) << _roundTripMsecs << " msec";
        cout << endl;
    }
}

// just a "things are happenin' indicator"
void Receiver::activityIndicator()
{
    static int charIdx = 0;
    static const char* actChars[] = {"//", "--", "\\\\", "||"};
    if (_sender) {
        cout << "   " << actChars[charIdx] << "\r" << flush;
        ++charIdx %= 4;
    }
}


void Receiver::accumulateBulkStats()
{
    if (_sender) {
        _kbpsOutStats.updateStats(_sender->getKbytePerSec());
        _kbpsInStats.updateStats(getKbytePerSec());
        _roundTripMSecsStats.updateStats(_roundTripMsecs);
    }
}

void Receiver::reportBulkStats()
{
    if (_sender) {
        // get the stats
        n_u::BasicRunningStats::StatResults outRateStats = _kbpsOutStats.getStats();
        n_u::BasicRunningStats::StatResults inRateStats = _kbpsInStats.getStats();
        n_u::BasicRunningStats::StatResults roundTripStats = _roundTripMSecsStats.getStats();

        // get rid of giant ugly number if no xmission and/or echo occurs.
        if (outRateStats._min > (std::numeric_limits<double>::max()/2))
            outRateStats._min = 0;
        if (inRateStats._min > (std::numeric_limits<double>::max()/2))
            inRateStats._min = 0;
        if (roundTripStats._min > (std::numeric_limits<double>::max()/2))
            roundTripStats._min = 0;
        
        cout << "Bulk Transmission Stats" << endl << "============================" << endl;
        cout << "Total Sender discarded: " << _sender->getDiscarded() << endl;
        cout << "Total Receiver bad packets: " << _totalBad << endl << endl;
        cout << "                   Min    Max    Mean StdDev" << endl;
        cout << "Out Rate (KBps): " << setw(6) << right << fixed
                                    << (outRateStats._min < 1.0 ? setprecision(4) : setprecision(1)) << outRateStats._min << "\t"
                                    << setw(6) << right << fixed
                                    << (outRateStats._max < 1.0 ? setprecision(4) : setprecision(1)) << outRateStats._max << "\t" 
                                    << setw(6) << right << fixed
                                    << (outRateStats._mean < 1.0 ? setprecision(4) : setprecision(1)) << outRateStats._mean  << "\t" 
                                    << setw(6) << right << fixed
                                    << (outRateStats._stddev < 1.0 ? setprecision(4) : setprecision(1)) << outRateStats._stddev << endl;
        cout << "In Rate (KBps):  " << setw(6) << right << fixed
                                    << (inRateStats._min < 1.0 ? setprecision(4) : setprecision(1)) << inRateStats._min << "\t"
                                    << setw(6) << right << fixed
                                    << (inRateStats._max < 1.0 ? setprecision(4) : setprecision(1)) << inRateStats._max << "\t" 
                                    << setw(6) << right << fixed
                                    << (inRateStats._mean < 1.0 ? setprecision(4) : setprecision(1)) << inRateStats._mean << "\t" 
                                    << setw(6) << right << fixed
                                    << (inRateStats._stddev < 1.0 ? setprecision(4) : setprecision(1)) << inRateStats._stddev << endl;
        cout << "Roundtrip (ms):  " << setw(6) << right << fixed << setprecision(0) << roundTripStats._min << "\t"
                                    << setw(6) << right << fixed << roundTripStats._max << "\t"
                                    << setw(6) << right << fixed << roundTripStats._mean << "\t"
                                    << setw(6) << right << fixed << roundTripStats._stddev << endl;
    }
}

int usage(const char* argv0)
{
    const char* cp = argv0 + strlen(argv0) - 1;
    for ( ; cp >= argv0 && *cp != '/'; cp--);
    argv0 = ++cp;
    cout << argv0 << " is used to stress the serial connections between two serial ports which are connected\n\
    with a null modem cable. It cycles through the known supported serial port types (RS232, RS422:full/half,\n\
    RS485:full/half) and sweeps the available baud rates to see where communication breaks down. It reports \n\
    bulk transmission statistics at the end of each test.\n\n\
Usage: " << argv0 << " [-d [1-3]] [-h] [-m [mode]] [-n N] [-o ttyopts] [-p] [-r rate] [-s size] [-t timeout] [-v] senddevice echodevice\n\
    -d: output debug information.\n\
    -h: display this help\n\
    -m: select a single port mode to use, RS232, RS422 (same as RS485_FULL), RS485_HALF\n\
    -n N: send N number of packets. Default of 0 means send until killed\n\
    -o ttyopts: SerialOptions string, see below. Default: " << defaultTermioOpts << "\n\
        Note that the port is always opened in raw mode, overriding whatever\n\
        the user specifies in ttyopts.\n\
    -p: send printable ASCII characters in the data portion, else send non-printable hex AA 55 F0 0F 00 FF\n\
    -r rate: send data packets at the given rate in Hz. Rate can be < 1.\n\
        This parameter is only needed on the sending side. The default rate\n\
        is calculated as 50% of the bandwidth, for the given the serial baud rate\n\
        and the size of the packets.\n\
    -s size: send extra data bytes in addition to normal 32 bytes.\n\
        This parameter is only needed on the sending side. Default size=0\n\
    -t timeout: receive timeout in seconds. Default=-1 and serstress calculates an appropriate timeout for the rate and\n\
        packet size. timeout=0 waits forever. timeout > 0 waits for timeout seconds before giving up.\n\
    -v: verbose, display sent and received data packets to stderr\n\
    -x: serial port mode to exclude. Default is RS485_HALF, unless it is specified in -m.\n\
    device: name of serial port to open, e.g. /dev/ttyDSM5\n\n\
ttyopts:\n  " << n_u::SerialOptions::usage() << "\n\
    Note that the port is always opened in raw mode, overriding what\n\
    the user specifies in ttyopts\n\n\
Example of testing a serial link starting at 115200 baud with modem control\n\
    lines (carrier detect) and hardware flow control. The transmitted\n\
    packets will be 2012+36=2048 bytes in length and will be\n\
    sent as fast as possible. The send side is connected to the echo side\n\
    with a null-modem cable:\n\n\
    " << argv0 << " -b 115200 -o n81mhr -s 2012 /dev/ttyDSM1 /dev/ttyDSM0\n\n\
Example of testing a serial link at 9600 baud without modem control\n\
    or hardware flow control. 36 byte packets be sent 10 times per second:\n\n\
    " << argv0 << " -b 9600 -m RS232 -o n81lnr -r 10 /dev/ttyDSM0 /dev/ttyDSM1\n\n\
Example of testing only RS485_HALF\n\n\
    " << argv0 << " -b 9600 -m RS485_HALF -o n81lnr -r 10 /dev/ttyDSM6 /dev/ttyDSM5"
<< endl;

    return 1;
}

int parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    if (argc < 3) {
        usage(argv[0]);
        exit(1);
    }
    while ((opt_char = getopt(argc, argv, "b:d:hm:n:o:pr:s:t:vx")) != -1) {
        switch (opt_char) {
        case 'b':
        {
            int startBaud = atoi(optarg);
            // cout << "startBaud:" << startBaud << " baudTableSize:" << baudTableSize;
            for (int i=1; i<baudTableSize; ++i) {
                if (n_u::Termios::bauds[i].rate == startBaud) {
                    // cout << "Found starting baud rate!" << endl;
                    baudStartIdx = i;
                    break;
                }
            }

            cout << "baudStartIdx: " << baudStartIdx << endl;
            if (!baudStartIdx) {
                cout << "Unknown starting baud rate!!" << endl;
                usage(argv[0]);
                exit(2);
            }
            break;
        }
        case 'd':
            debug = atoi(optarg);
            break;
        case 'h':
            return usage(argv[0]);
            break;
        case 'm': 
            portType = n_c::SerialXcvrCtrl::strToPortType(optarg);
            cout << "serstress: cmd line parsing: \'m\': " << n_c::SerialXcvrCtrl::portTypeToStr(portType) << endl;
            if (portType == n_c::RS485_HALF) {
                // don't skip the default skipped port type
                skipped_portType = n_c::LOOPBACK;
            }
            break;
        case 'n':
            nPacketsOut = atoi(optarg);
            break;
        case 'o':
            termioOpts = optarg;
            break;
        case 'p':
            ascii = true;
            break;
        case 'r':
            rate = atof(optarg);
            break;
        case 's':
            dataSize = atoi(optarg);
            break;
        case 't':
            timeoutSecs = atoi(optarg);
            break;
        case 'v':
            verbose = true;
            break;
        case 'x':
            skipped_portType = n_c::SerialXcvrCtrl::strToPortType(optarg);
            if (skipped_portType == n_c::LOOPBACK) {
                cout << "-x option specifies unknown port type to skip." << endl << endl;
                return usage(argv[0]);
            }
            break;
        case '?':
            return usage(argv[0]);
        }
    }

    if (optind == argc - 2) {
        device = string(argv[optind++]);
        echoDevice = string(argv[optind++]);
        if (device == echoDevice || device.length() == 0 || echoDevice.length() == 0) {
            return usage(argv[0]);
        }
    }
    if (optind != argc) return usage(argv[0]);

    return 0;
}

void openPort(bool isSender, int& rcvrTimeout) throw(n_u::IOException, n_u::ParseException)
{
    n_c::SerialPortIODevice& myPort = isSender ? port : echoPort;
    n_c::PortConfig myPortConfig = isSender ? portConfig : echoPortConfig;
    string& myShortName = isSender ? shortName : echoShortName;
    string& myDevice = isSender ? device : echoDevice;

    // myPort.setName(myDevice);

    // remove common "/dev/tty" prefix from device name to shorten output
    myShortName = myDevice;
    if (myShortName.substr(0,8) == "/dev/tty")  myShortName = myShortName.substr(8);

    myPort.setBlocking(false);

    try { 
        myPort.open(O_RDWR | O_NOCTTY | O_NONBLOCK); 
    }
    catch (n_u::IOException& e)    {
        throw n_u::Exception(std::string("serstress: port open error: " + myPort.getName() + e.what()));
    }

    GPIO_PORT_DEFS sensorPortID = isSender ? port.getPortConfig().xcvrConfig.port : echoPort.getPortConfig().xcvrConfig.port;
    n_u::SensorPowerCtrl sensorPower(sensorPortID);
    sensorPower.enablePwrCtrl(true);
    sensorPower.pwrOn();

    myPort.setPortConfig(myPortConfig);
    myPort.applyPortConfig();


    cout << endl << "Testing " << (isSender ? "Sender " : "Echo ") << "Port Configuration" << endl << "======================" << endl;
    myPort.getPortConfig().print();
    sensorPower.print();

    int setBaud = myPortConfig.termios.getBaudRate() * 1.0;
    int bytesPerPacket = MIN_PACKET_LENGTH + dataSize;
    int dataBits = myPortConfig.termios.getDataBits();
    int stopBits = myPortConfig.termios.getStopBits();
    int parityBits = myPortConfig.termios.getParity() == n_u::Termios::NONE ? 0 : 1;
    float bytesPerSec = setBaud / ((dataBits + stopBits + parityBits) * 1.0);

    float calcRate = rate;
    float fullPcktRate = bytesPerSec / bytesPerPacket;
    // rate > greater than 0 comes from command line, so use it
    // else calculate a reasonable rate. Since this is a stress test, it should be 
    // at least the full rate.
    if (rate <= 0.0) {
        if (rate < -1.0) {
            calcRate = fullPcktRate;

        }
        else {
            calcRate = fullPcktRate/2.0;
        }
    }

    // Always assume we can write to the buffer far faster than the serial port can send it...
    fesetround(FE_TONEAREST);
    periodMsec = static_cast<int>(rint(1000.0/calcRate));
    // timeoutSecs value >= 0 comes from the command line, use it.
    // else calculate it.
    if (timeoutSecs < 0) {
        // timeout is based on how long it takes an entire packet to get to the receiver
        // so it indicates a sender stall or other comm error.
        // double it and then some to handle the roundtrip case for the sender receiver.
        rcvrTimeout = max( 1, static_cast<int>(3*bytesPerPacket/bytesPerSec)+1);
    }

    else {
        rcvrTimeout = timeoutSecs;
    }

    // only want to report this once
    if (isSender) { 
        cout << std::endl << "Packet send rate = " << fixed;
        if (calcRate < 0.5) {
            cout << setprecision(4);
        }
        else {
            cout << setprecision(1);
        } 
        cout << calcRate <<
            " Hz, which is " << fixed << setprecision(1) <<
                calcRate/fullPcktRate * 100.0 <<
                "% of the bandwidth at baud=" << setBaud <<  " bps" <<
                " and a packet length=" << bytesPerPacket << " bytes" << endl << endl;

        cout << "Interpacket Delay (mS): " << periodMsec << endl;
        cout << "Receiver timeout (sec): " << rcvrTimeout << endl << endl;
    }

    if (myPort.getPortType() == n_c::RS485_HALF) {
        ILOG(("Port is set to RS485_HALF"));
        // Need to set RTS low so that no xmission occurs until needed. This allows 
        // the sensor to send data until the DSM has something to say to it.
        // NOTE: Have to set RTS flag high to get it set low on output.
        //       For some reason, the API logic sets it high, when the arguement is -1.
        if (isSender) {
            myPort.setRTS485(-1);
            ILOG(("Setting Sender Port to RTS485 mode: ") << myPort.getRTS485());
        }

        else {
            myPort.setRTS485(-1);
            ILOG(("Setting Echo Port to RTS485 mode: ") << myPort.getRTS485() );
        }
    }

    int modembits = myPort.getModemStatus();
    cout << "On Open " << (isSender ? "Sender " : "Echo ") << "Modem flags: " << myPort.modemFlagsToString(modembits) << endl;

    myPort.flushBoth();
}

void closePort(bool isSender) throw(n_u::IOException, n_u::ParseException)
{
    n_c::SerialPortIODevice& myPort = isSender ? port : echoPort;
    myPort.flushBoth();
    myPort.close();

    GPIO_PORT_DEFS sensorPortID = isSender ? port.getPortConfig().xcvrConfig.port : echoPort.getPortConfig().xcvrConfig.port;
    n_u::SensorPowerCtrl sensorPower(sensorPortID);
    sensorPower.enablePwrCtrl(true);
    sensorPower.pwrOff();
}

static void sigAction(int sig, siginfo_t* siginfo, void*) {

    cout <<
    	"serstress received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) <<
	", interrupted=" << interrupted << endl;
#ifdef DEBUG
#endif

    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            if (interrupted++ > 1) exit(1);
    break;
    }
}

static
void setupSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGINT);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);

    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

int main(int argc, char**argv)
{
    cout << endl << "****************************************" << endl;
    cout         << "**        SerStress Test Start        **" << endl;
    cout         << "****************************************" << endl;

    n_c::PORT_TYPES portTypeList[] = {n_c::RS232, n_c::RS422, n_c::RS485_HALF};

    int status = 0;
    int calcRecvrTimeout = -1;
    int senderPortNum = -1;
    int echoPortNum = -1;

    // determine size of baud table since it's static in class Terimios, and only the declaration is available
    // !!!needed *before* parsing cmd line opts!!!
    for (int k=1; n_u::Termios::bauds[k].rate > 0; ++k, ++baudTableSize);

    if (parseRunstring(argc,argv)) return 1;

    Logger* logger = 0;
    LogScheme scheme;

    if (debug) {
        cout << "Debug selected; setting up Logger" << endl;
        // Set a default for SerialXcvrCtrl notifications only.
        logger = Logger::getInstance();
        scheme = logger->getScheme("sing_default");
        scheme.addConfig(LogConfig("level=info"));
        if (debug >= 1) scheme.addConfig(LogConfig("level=notice"));
        if (debug >= 2) scheme.addConfig(LogConfig("level=debug"));
        logger->setScheme(scheme);
    }

    // If it's still 0, then baud wasn't set on the command line.
    // We need to start at idx == 1, because the first baud is 0.
    if (baudStartIdx == 0) {
        ++baudStartIdx;
    }

    cout << endl << "Serial Option String: " << termioOpts << endl;
    setupSignals();

    if (device.substr(0,11) == "/dev/ttyDSM"){

        istringstream(device.substr(11)) >> senderPortNum;
        port.setName(device);
    }
    else {
        CLOG(("Didn't find sender device: ") << device);
        exit(3);
    }

    if (echoDevice.substr(0,11) == "/dev/ttyDSM"){
        istringstream(echoDevice.substr(11)) >> echoPortNum;
        echoPort.setName(echoDevice);
    }
    else {
        CLOG(("Didn't find echo device: ") << echoDevice);
        exit(3);
    }

    ILOG(("Setting up port type for Sender port: ") << senderPortNum);
    ILOG(("Setting up port type for Echo port: ") << echoPortNum);

    // save a virgin copy and prepend the baud rate later.
    string tempTermiosOpts = termioOpts;

    // if portType != LOOPBACK and portType != RS485_HALF, then use it and loop forever-ish...
    // otherwise just loop through the port types list
    int looptest = sizeof(portTypeList)/sizeof(portTypeList[0]);
    if (portType != n_c::LOOPBACK && portType != n_c::RS485_HALF) {
        looptest = INT_MAX;
    }
    if (portType == n_c::RS485_HALF) {
        looptest = 1;
    }


    DLOG(("portType == ") << n_c::SerialXcvrCtrl::portTypeToStr(portType) << ", looptest == " << looptest);

    for (int i=0; i<looptest; ++i) {
        n_c::PORT_TYPES nextPortType = portType;
        // change port type only if it wasn't specified on the command line
        if (looptest != INT_MAX && nextPortType != n_c::RS485_HALF) {
            nextPortType = portTypeList[i];

            // primarily used to check for RS485_HALF as this isn't curren't supported in 
            // the hardware.
            if (nextPortType == skipped_portType) {
                continue;
            }
            DLOG(("Changing the port type because it wasn't specified on the command line: ") << nextPortType);
        }

        for (int j=baudStartIdx; j < baudTableSize; ++j) { // skip 0 baud!!
            cout << endl << "****************************************" << endl;
            cout         << "****************************************" << endl;

            // convert rate integer to string
            ostringstream baudStr;
            baudStr << n_u::Termios::bauds[j].rate;
            termioOpts = baudStr.str();
            termioOpts.append(tempTermiosOpts);

            cout << "termioOpts: " << termioOpts << endl;
            n_u::SerialOptions options;
            options.parse(termioOpts);

            portConfig.termios = options.getTermios();
            portConfig.termios.setRaw(true);
            portConfig.termios.setRawLength(1);
            portConfig.termios.setRawTimeout(0);
            portConfig.xcvrConfig.port = static_cast<n_u::GPIO_PORT_DEFS>(senderPortNum);
            portConfig.xcvrConfig.portType = nextPortType;

            // echo PortConfig is identical, but for the port ID
            echoPortConfig = portConfig;
            if (echoPortConfig != portConfig) {
                throw Exception("Sender and Echo PortConfigs don't match!!!");
            }
            echoPortConfig.xcvrConfig.port = static_cast<n_u::GPIO_PORT_DEFS>(echoPortNum);

            /*************************************************************************
            ** TODO: RS485 half duplex? Set the GPIO to short Bulgin pins 3&4, and 5&6
            *************************************************************************/

            try {
                openPort(SENDING, calcRecvrTimeout);
                openPort(ECHOING, calcRecvrTimeout);
            }
            catch(const n_u::IOException &e) {
                cout << "Error: " << e.what() << endl;
                return 1;
            }
            catch(const n_u::ParseException &e) {
                cout << "Error: " << e.what() << endl;
                return 1;
            }

            Receiver echoRcvr(calcRecvrTimeout, 0);
            try {
                ILOG(("Starting Echo Recvr..."));
                echoRcvr.start();
            }
            catch(const n_u::IOException &e) {
                cout << "Error: " << e.what() << endl;
                status = 1;
            }

            n_u::auto_ptr<Sender> sender(new Sender(ascii,dataSize));
            ILOG(("Starting Sender..."));
            sender->start();

            Receiver sendRcvr(calcRecvrTimeout, sender.get());
            try {
                ILOG(("Starting Send Recvr"));
                sendRcvr.start();
            }
            catch(const n_u::IOException &e) {
                cout << "Error: " << e.what() << endl;
                status = 1;
            }

            while (sender->isRunning())
                sleep(1);

            cout << "Cleaning up threads..." << endl;

            if (sender.get()) {
                sender->interrupt();
                try {
                    sender->join();
                }
                catch(const n_u::Exception &e) {
                    cout << "Error: " << e.what() << endl;
                    status = 1;
                }
            }
            ILOG(("Sender thread exited..."));

            while (echoRcvr.isRunning())
            {
                sleep(1);
            }

            try {
                echoRcvr.join();
            }
            catch(const n_u::Exception &e) {
                std::cout << "Error: " << e.what() << endl;
                status = 1;
            }

            // while(echoRcvr.isRunning()) ;

            ILOG(("Echo Receive thread exited..."));

            while (sendRcvr.isRunning()) {
                sleep(1);
            }

            try {
                sendRcvr.join();
            }
            catch(const n_u::Exception &e) {
                std::cout << "Error: " << e.what() << endl;
                status = 1;
            }

            // while(sendRcvr.isRunning()) ;

            ILOG(("Sender Receive thread exited..."));

            interrupted = 0;

            try {
                closePort(SENDING);
                closePort(ECHOING);
            }
            catch(const n_u::IOException &e) {
                cout << "Error: " << e.what() << endl;
                return 1;
            }
            catch(const n_u::ParseException &e) {
                cout << "Error: " << e.what() << endl;
                return 1;
            }

            cout << endl << "Finished test run for:" << endl;
            port.getPortConfig().print(true);
            cout << endl;

            sendRcvr.reportBulkStats();
        }
    }

    cout << endl << "****************************************" << endl;
    cout         << "**        SerStress Test End          **" << endl;
    cout         << "****************************************" << endl;

    return status;
}

static int __attribute__((__unused__)) cksum_test(int, char**)
{
    unsigned char buf[65536];

    FILE* fp = fopen("cksum.c","r");
    if (! fp) {
        perror("cksum.c");
        return 1;
    }

    int len = fread(buf,1,sizeof(buf),fp);
    fclose(fp);

    uint32_t crc = cksum(buf,len);

    printf("%u %08x %d\n",crc,crc,len);
    return 0;
}

