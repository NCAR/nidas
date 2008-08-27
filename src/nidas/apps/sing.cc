/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2008-08-02 16:16:18 -0600 (Sat, 02 Aug 2008) $

    $LastChangedRevision: 4228 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/apps/data_dump.cc $

    sing: a serial ping for testing serial connections.
 ********************************************************************

*/

#include <nidas/util/SerialPort.h>
#include <nidas/util/SerialOptions.h>
#include <nidas/util/Thread.h>
#include <nidas/util/util.h>
#include <nidas/core/Looper.h>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip>
#include <limits.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <cstdlib>

using namespace std;
namespace n_u = nidas::util;

static float rate = 999999;
static bool isSender = true;
static bool ascii = false;
static int dataSize = 0;
static unsigned int nPacketsOut = 999998;
static string defaultTermioOpts = "9600n81lnr";
static string termioOpts = defaultTermioOpts;
static string device;
static bool verbose = false;
static int timeoutSecs = 0;
static int interrupted = 0;
static unsigned int periodMsec = 0;

static n_u::SerialPort port;

/**
 * Format of test packet:
 * NNNNNN MMMMMMMMM ccccc <data>HHHHHHHH\x04
 * NNNNNN: packet number starting at 0, 6 ASCII digits, followed by space.
 * MMMMMMMM: number of milliseconds since program start:
 *      9 digits followed by a space.
 * ccccc: data portion size, in bytes, 0 or larger.
 *      5 digits, followed by space.
 * <data>: data portion of packet, may be 0 bytes in length.
 * HHHHHHHH: CRC of packet contents, up to but not including CRC (duh),
 *      8 ASCII hex digits.
 * \x04: trailing ETX byte
 * Length of packet is then 7 + 10 + 6 + dataSize + 8 + 1 = 32 + dataSize
 */

/*
 * Tests:
 *  sender: ttyS0
 *  receiver ttyUSB0 (keyspan converter)
 *      sender gets into state where it sends a bunch, then
 *      receives a bunch, not interleaved.
 *      sent# gets ahead of rcvd# by as much as 150
 *      If one kills receiver first, cannot kill sender.
 *      Sender seems to be stuck in a write.
 *  sender: ttyUSB0 (keyspan converter)
 *  receiver ttyS0
 *      packets are much more interleaved
 *      sent# only gets ahead of rcvd# by less than 10.
 */
const int START_OF_DATA = 23;   // data starts at byte 27
const int LENGTH_OF_CRC = 8;
const int MIN_PACKET_LENGTH = START_OF_DATA + LENGTH_OF_CRC + 1;
int MAX_PACKET_LENGTH = 65535;
int MAX_DATA_LENGTH = MAX_PACKET_LENGTH - MIN_PACKET_LENGTH;

char ETX = '\x04';
int EOF_NPACK = 999999;

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
};

class Receiver
{
public:
    Receiver(int timeoutSecs,const Sender*);

    void run() throw(n_u::IOException);
    void report();

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
    int _timeoutSecs;
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

    const Sender* _sender;
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
    _sec0(0),_msec0(0),_msec100ago(0),_byteSum(0),_deltaT(0),_discarded(0)
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
        unsigned char seq[] = {'\xaa','\x55','\xf0','\x0f','\x00','\xff'};
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
    for (;_nout < nPacketsOut;) {
        if (isInterrupted() || interrupted) break;
        send();
        if (periodMsec > 0) nidas::core::Looper::sleepUntil(periodMsec);
    }
    _nout = EOF_NPACK;
    send();
    flush();
    return RUN_OK;
}

void Sender::flush() throw(n_u::IOException)
{
    static int wz = 0;
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

    if (_hptr + _packetLength + 1 > _eob) flush();
    if (_hptr + _packetLength + 1 > _eob) {
        if (!(_discarded++ % 100))
            cerr << "Discarded " << _discarded << " packets because Senders output buffer is full" << endl;
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
    if (verbose) cerr << "sent " << _packetLength << ":" << 
            n_u::addBackslashSequences(string(_hptr,_packetLength)) << endl;

    _byteSum -= _last100[iout];
    if (_msec100[iout] > 0)
        _msec100ago = _msec100[iout];
    _deltaT = msec - _msec100ago;
    _byteSum += _packetLength;
    _msec100[iout] = msec;
    _last100[iout] = _packetLength;

    _hptr += _packetLength;
    if (_hptr + 1 == _eob) flush();
    _nout++;

}

Receiver::Receiver(int timeoutSecs,const Sender*s):
    RBUFLEN(8192),_buf(0),_rptr(0),_wptr(0),_eob(0),_buflen(0),
    _timeoutSecs(timeoutSecs),_msec100ago(0),_ngood10(0),_ngood100(0),
    _Npack(0),_Nlast(0),_dsize(0),_dsizeTrusted(0),
    _msec(0),_scanHeaderNext(true),_sec0(0),_msec0(0),
    _byteSum(0),_deltaT(0),_roundTripMsecs(0),
    _sender(s)
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

void Receiver::run() throw(n_u::IOException)
{
    fd_set readfds;
    FD_SET(port.getFd(),&readfds);
    int nfds = port.getFd() + 1;
    struct timeval timeout;

    for (; !interrupted; ) {
        timeout.tv_sec = timeoutSecs;
        timeout.tv_usec = 0;
        fd_set fds = readfds;

        int nfd = select(nfds,&fds,0,0,(timeoutSecs > 0 ? &timeout : 0));

        if (nfd < 0) {
            if (errno == EINTR) return;
            throw n_u::IOException(device,"select",errno);
        }
        if (nfd == 0) {
            cerr << device << ": timeout " << timeoutSecs <<
                " seconds" << endl;
            report();
            return;
        }
        int len = _eob - _wptr;
        assert(len > 0);
        int l = port.read(_wptr,len);
        if (l == 0) break;
        if (verbose) cerr << "rcvd " << l << ":" <<
            n_u::addBackslashSequences(string(_wptr,l)) << endl;
        _wptr += l;
        if (scanBuffer()) break;
    }
}

/**
 * Return 0 on normal scan, 1 on receipt of EOF packet.
 */
int Receiver::scanBuffer()
{
    struct timeval tnow;

    int plen;

    for (;;) {
        bool goodPacket = false;
        if (_scanHeaderNext) {        // read header
            if (_wptr - _rptr < START_OF_DATA) break;   // not enough data
            if (sscanf(_rptr,"%u %u %u",&_Npack,&_msec,&_dsize) == 3 &&
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
                if (!_sender) port.write(_rptr,plen);     // echo back
                if (_Npack == EOF_NPACK) return 1;
                _dsizeTrusted = _dsize;
                _rptr += plen;

            }
            _scanHeaderNext = true;
        }
        if (!goodPacket) {
            // bad packet, look for ETX to try to make some sense of this junk
            for ( ; _rptr < _wptr && *_rptr++ != ETX; );
        }
        else report();
    }
    unsigned int l = _wptr - _rptr;  // unscanned characters in buffer
    if (l > 0) memmove(_buf,_rptr,l);
    _rptr = _buf;
    _wptr = _buf + l;

    // Reset the buffer size to something reasonable if it
    // grew because of a bad dsize.
    if (_buflen > RBUFLEN) {
        int blen = std::max(MIN_PACKET_LENGTH + _dsizeTrusted,_wptr - _rptr);
        blen = std::max(blen,RBUFLEN);
        if (blen != _buflen) reallocateBuffer(blen);
    }
    return 0;
}

void Receiver::report()
{
    if (_sender) cout << "sent#:" << setw(5) << _sender->getNout() << ' ';
    cout << "rcvd#:" << setw(5) << _Nlast <<
        ", " << setw(2) << _ngood10 << '/' << std::min(_Nlast+1,(unsigned int)10) <<
        ", " << setw(3) << _ngood100 << '/' << std::min(_Nlast+1,(unsigned int)100);
    if (_sender) {
        float kbps = _sender->getKbytePerSec();
        cout << ", out:" << setw(4) << setprecision(4) << kbps << " kB/sec";
    }
    float kbps = getKbytePerSec();
    cout << ", in:" << setw(4) << setprecision(4) << kbps << " kB/sec";
    if (_sender) cout << ", dT:" << setw(4) << _roundTripMsecs << " msec";
    cout << endl;
}

int usage(const char* argv0)
{
    const char* cp = argv0 + strlen(argv0) - 1;
    for ( ; cp >= argv0 && *cp != '/'; cp--);
    argv0 = ++cp;
    cerr << "\
Usage: " << argv0 << " [-e] [-h] [-n N] [-o ttyopts] [-p] [-r rate] [-s size] [-t timeout] [-v] device\n\
  -e: echo packets back to sender.\n\
  -h: display this help\n\
  -n N: send N number of packets. Default of 0 means send until killed\n\
  -o ttyopts: SerialOptions string, see below. Default: " << defaultTermioOpts << "\n\
     Use raw tty mode because the exchanged packets don't contain CR or NL\n\
  -p: send printable ASCII characters in the data portion, else send non-printable hex AA 55 F0 0F 00 FF\n\
  -r rate: send data at the given rate in Hz. Rate can be < 1.\n\
    This parameter is only needed on the sending side. Default rate=1 Hz\n\
  -s size: send extra data bytes in addition to normal 36 bytes.\n\
    This parameter is only needed on the sending side. Default size=0\n\
  -t timeout: receive timeout in seconds. Default=0 (forever)\n\
  -v: verbose, display sent and received data packets to stderr\n\
  device: name of serial port to open, e.g. /dev/ttyS5\n\n\
ttyopts:\n  " << n_u::SerialOptions::usage() << "\n\n\
At one end of the serial connection run \"" << argv0 << "\" and at the\n\
other end, either run \"" << argv0 << " -e\" or use a loopback device.\n\n\
Example of testing a serial link at 115200 baud with modem control\n\
    lines (carrier detect) and hardware flow control. The transmitted\n\
    packets will be 2012+36=2048 bytes in length and will be\n\
    sent as fast as possible. The send side is connected to the echo side\n\
    with a null-modem cable:\n\
Send side: " << argv0 << " -o 115200n81mhr -s 2012 /dev/ttyS5\n\
Echo side: " << argv0 << " -e -o 115200n81mhr /dev/ttyS6\n\n\
Example of testing a serial link at 9600 baud without modem control\n\
    or hardware flow control. 36 byte packets be sent 10 times per second:\n\
Send side: " << argv0 << " -o 9600n81lnr -r 10 /dev/ttyS5\n\
Echo side: " << argv0 << " -e -o 9600n81lnr /dev/ttyS6" << endl;
    return 1;
}

int parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "ehn:o:pr:s:t:v")) != -1) {
        switch (opt_char) {
        case 'e':
            isSender = false;
            break;
        case 'h':
            return usage(argv[0]);
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
            periodMsec = (unsigned int) rint(MSECS_PER_SEC / rate);
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
        case '?':
            return usage(argv[0]);
        }
    }
    if (optind == argc - 1) device = string(argv[optind++]);
    if (device.length() == 0) return usage(argv[0]);
    if (optind != argc) return usage(argv[0]);

    return 0;
}

void openPort() throw(n_u::IOException, n_u::ParseException)
{
    n_u::SerialOptions options;
    options.parse(termioOpts);

    port.setName(device);
    port.setOptions(options);
    port.setRaw(true);
    port.setRawLength(1);
    port.setRawTimeout(0);
    port.setBlocking(true);
    port.open(O_RDWR | O_NOCTTY | O_NONBLOCK);
    // port.setTermioConfig();
    // cerr << "port opened" << endl;
    int modembits = port.getModemStatus();
    cerr << "Modem flags: " << port.modemFlagsToString(modembits) << endl;
    port.flushBoth();
}

static void sigAction(int sig, siginfo_t* siginfo, void* vptr) {

    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
#ifdef DEBUG
#endif

    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            if (interrupted++ > 0) exit(1);
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
    if (parseRunstring(argc,argv)) return 1;

    setupSignals();

    try {
        openPort();
    }
    catch(const n_u::IOException &e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    catch(const n_u::ParseException &e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    std::auto_ptr<Sender> sender;

    if (isSender) {
        sender.reset(new Sender(ascii,dataSize));
        sender->start();
    }

    Receiver rcvr(timeoutSecs,sender.get());
    try {
        rcvr.run();
    }
    catch(const n_u::IOException &e) {
        cerr << "Error: " << e.what() << endl;
    }

    if (sender.get()) {
        sender->interrupt();
        try {
            sender->join();
        }
        catch(const n_u::Exception &e) {
            cerr << "Error: " << e.what() << endl;
        }
    }
}

static int __attribute__((__unused__)) cksum_test(int argc, char** argv)
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
}

