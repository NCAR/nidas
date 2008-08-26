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

static float rate = 1.0;
static bool isSender = true;
static bool ascii = false;
static int dataSize = 0;
static unsigned int nPacketsOut = UINT_MAX;
static string defaultTermioOpts = "9600n81lnr";
static string termioOpts = defaultTermioOpts;
static string device;
static bool verbose = false;
static int timeoutSecs = 0;
static bool interrupted = false;
static unsigned int periodMsec = 0;

static n_u::SerialPort port;

/**
 * Format of test packet:
 * NNNNNN SSSSSS UUUUUU ccccc <data>HHHHHHHH\x04
 * NNNNNN: packet number starting at 0, 6 ASCII digits, followed by space.
 * SSSSSS UUUUUU: number of seconds and microseconds since program start:
 *      6 digits, space 6 digits.
 * ccccc: data portion size, in bytes, 0 or larger.
 *      5 digits, followed by space.
 * <data>: data portion of packet, may be 0 bytes in length.
 * HHHHHHHH: CRC of packet contents, up to but not including CRC (duh),
 *      8 ASCII hex digits.
 * \x04: trailing ETX byte
 * Length of packet is then (6 + 1) * 3 + 6 + dataSize + 8 + 1 = 36 + dataSize
 */

const int START_OF_DATA = 27;   // data starts at byte 27
const int LENGTH_OF_CRC = 8;
const int MIN_PACKET_LENGTH = START_OF_DATA + LENGTH_OF_CRC + 1;
int MAX_PACKET_LENGTH = 65535;
int MAX_DATA_LENGTH = MAX_PACKET_LENGTH - MIN_PACKET_LENGTH;

char ETX = '\x04';

class Sender: public n_u::Thread
{
public:
    Sender(bool ascii,int size);
    ~Sender();
    int run() throw(n_u::Exception);
    void send() throw(n_u::IOException);
    unsigned int getNout() const { return _nout; }
    time_t getSecOffset() const { return _sec0; }

private:
    bool _ascii;
    int _dsize;
    char* _buf;
    unsigned int _nout;
    time_t _sec0;
    int _packetLength;
};

class Receiver
{
public:
    Receiver(int timeoutSecs,const Sender*);

    void run() throw(n_u::IOException);
    void report();

private:

    void reallocateBuffer(int len);

    char* _buf;
    char* _bufptr;
    char* _eob;
    int _buflen;
    int _timeoutSecs;
    vector<int> _last10;
    vector<int> _last100;
    vector<float> _rateVec;
    int _ngood10;
    int _ngood100;
    double _kbytePerSecSum;
    unsigned int _Nlast;

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
    _buf(0),_nout(0)
{
    _packetLength = START_OF_DATA + _dsize + LENGTH_OF_CRC + 1;

    // provide space for sprintf's trailing null (but null is not sent).
    _buf = new char[_packetLength + 1];

    struct timeval tv;
    gettimeofday(&tv,0);
    _sec0 = tv.tv_sec;

    if (_ascii) {
        for (int i = 0; i < _dsize; i++) {
            char c = ((i % 52) < 26) ? 'A' + (i % 26) : 'a' + (i % 26);
            _buf[START_OF_DATA + i] = c;
        }
    }
    else {
        unsigned char seq[] = {'\xaa','\x55','\xf0','\x0f','\x00','\xff'};
        for (int i = 0; i < _dsize; i++) {
            char c = seq[i % sizeof(seq)];
            _buf[START_OF_DATA + i] = c;
        }
    }
}

Sender::~Sender()
{
    delete [] _buf;
}

int Sender::run() throw(n_u::Exception)
{
    for (;_nout < nPacketsOut;) {
        if (isInterrupted()) break;
        send();
        if (periodMsec > 0) nidas::core::Looper::sleepUntil(periodMsec);
    }
    return RUN_OK;
}

void Sender::send() throw(n_u::IOException)
{
    struct timeval tv;
    gettimeofday(&tv,0);
    int icrc = START_OF_DATA + _dsize;

    char csave = _buf[START_OF_DATA];
    sprintf(_buf,"%6u %6ld %6lu %5u ",_nout++,tv.tv_sec - _sec0,
        tv.tv_usec,_dsize);
    _buf[START_OF_DATA] = csave;

    uint32_t crc = cksum((const unsigned char*)_buf,icrc);
    sprintf(_buf + icrc,"%08x%c",crc,ETX);

    port.write(_buf,_packetLength);
    if (verbose) cerr << "sent " << _packetLength << ":" << _buf << endl;
}

Receiver::Receiver(int timeoutSecs,const Sender*s):
    _buf(0),_bufptr(0),_buflen(0),
    _timeoutSecs(timeoutSecs),_ngood10(0),_ngood100(0),
    _kbytePerSecSum(0.0),_Nlast(0),_sender(s)
{
    _last10.resize(10);
    _last100.resize(100);
    _rateVec.resize(100);
    reallocateBuffer(MIN_PACKET_LENGTH + 1);
}
void Receiver::reallocateBuffer(int len)
{
    if (len > _buflen) {
        char* newbuf = new char[len];
        if (_buf) memcpy(newbuf,_buf,_buflen);
        delete [] _buf;
        _bufptr = newbuf + (_bufptr - _buf);
        _buf = newbuf;
        _buflen = len;
        _eob = _buf + len;
    }
}

void Receiver::run() throw(n_u::IOException)
{
    fd_set readfds;
    FD_SET(port.getFd(),&readfds);
    int nfds = port.getFd() + 1;
    struct timeval timeout;
    struct timeval tnow;

    unsigned int Npack = 0;
    time_t sec = 0;
    suseconds_t usec = 0;
    int dsize = -1;

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
        int len = _eob - _bufptr;
        int plen = MIN_PACKET_LENGTH + (dsize < 0 ? 0 : dsize);
        if (plen > len) {
            reallocateBuffer(_buflen + plen - len);
            len = _eob - _bufptr;
        }
        int l = port.read(_bufptr,len);
        if (verbose) cerr << "rcvd " << l << ":" << string(_bufptr,l) << endl;
        _bufptr += l;

        bool goodPacket = false;

        if (dsize < 0) {        // read header
            if (_bufptr - _buf < START_OF_DATA) continue;
	    unsigned int isec, iusec;
            if (sscanf(_buf,"%u %u %u %u",&Npack,&isec,&iusec,&dsize) == 4) {
		sec = isec;
		usec = iusec;

                // if packet skipped
                for (unsigned int n = _Nlast + 1; n < Npack; n++) {
                    if (_ngood10 > 0 && _last10[n % 10] == 1) _ngood10--;
                    _last10[n % 10] = 0;
                    if (_ngood100 > 0 && _last100[n % 100] == 1) _ngood100--;
                    _last100[n % 100] = 0;
                    if (_sender) {
                        _kbytePerSecSum -= _rateVec[n % 100];
                        _rateVec[n % 100] = 0.0;
                    }
                }
                _Nlast = Npack;
            }
        }
        // At this point if dsize < 0 the header was not parseable.
        if (dsize >= 0) {
            if (dsize < MAX_DATA_LENGTH) {
                plen = MIN_PACKET_LENGTH + dsize;
                if (_bufptr - _buf < plen) continue;

                if (_sender) gettimeofday(&tnow,0);
                uint32_t crc = cksum((const unsigned char*)_buf,START_OF_DATA + dsize);
                uint32_t incrc = 0;
                if (_buf[START_OF_DATA + dsize + LENGTH_OF_CRC] == ETX &&
                    sscanf(_buf+START_OF_DATA+dsize,"%8x",&incrc) == 1 &&
                    incrc == crc) {
                    // cerr << "good: Nlast=" << _Nlast << " dsize=" << dsize << endl;
                    goodPacket = true;
                    // cerr << "Nlast=" << _Nlast << " _last10[]=" <<
                      //   _last10[_Nlast % 10] << " ngood10=" << _ngood10 << endl;
                        
                    if (_last10[_Nlast % 10] == 0) _ngood10++;
                    _last10[_Nlast % 10] = 1;
                    if (_last100[_Nlast % 100] == 0) _ngood100++;
                    _last100[_Nlast % 100] = 1;
                    if (_sender) {
                        float dtsec =
                            (tnow.tv_sec - _sender->getSecOffset() - sec) +
                                (float)(tnow.tv_usec - usec) / USECS_PER_SEC;
                        float kbps = plen / dtsec / 1000.0;
                        _kbytePerSecSum -= _rateVec[_Nlast % 100];
                        _rateVec[_Nlast % 100] = kbps;
                        _kbytePerSecSum += kbps;
                        // cerr << "kpbs=" << kbps << " sum=" << _kbytePerSecSum << endl;
                    }
                    else port.write(_buf,plen);     // echo back
                    _bufptr = _buf;
                }
            }
            else cerr << "Excessive data length: " << dsize <<
                "bytes. Packet skipped." << endl;
            dsize = -1;     // read header next
        }
        if (!goodPacket) {  // bad packet, look for ETX to try to
                            // make some sense of this junk
            char* cp = _buf;
            for ( ; cp < _bufptr && *cp++ != ETX; );
            l = _bufptr - cp;
            if (l >= MAX_PACKET_LENGTH) l = 0;   // hopeless
            // move data after ETX to beg of buf
            if (l > 0) memmove(_buf,cp,l);
            _bufptr = _buf + l;
        }
        report();
        // goodPacket: _bufptr will == _buf, buffer is empty
        // badPacket:
        //      data left in buffer will be that found after ETX.
        //      Size of data in buffer is < MAX_PACKET_LENGTH.
        // incomplete packet:
        //      size of data in buffer is less than START_OF_DATA, or
        //          MIN_PACKET_LENGTH + dsize.
    }
}
void Receiver::report()
{
    if (_sender) cout << "sent#:" << setw(5) << _sender->getNout() << ' ';
    cout << "rcvd#:" << setw(5) << _Nlast <<
        ", " << setw(2) << _ngood10 << '/' << std::min(_Nlast+1,(unsigned int)10) <<
        ", " << setw(3) << _ngood100 << '/' << std::min(_Nlast+1,(unsigned int)100);
    if (_sender) {
        float kbps = _Nlast == 0 ? 0.0 :
            _kbytePerSecSum / (_Nlast > 100 ? 100 : _Nlast);
        cout << ", " << setprecision(2) << kbps << " kB/sec";
        // cout << ", Nlast=" << _Nlast << " sum=" << _kbytePerSecSum;
        if (!(_Nlast % 100)) {
            _kbytePerSecSum = 0.0;
            for (unsigned int i = 0; i < 100; i++)
                _kbytePerSecSum += _rateVec[i];
        }
    }
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
Echo size: " << argv0 << " -e -o 9600n81lnr /dev/ttyS6" << endl;
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
    port.open(O_RDWR | O_NOCTTY);
    port.setOptions(options);
    port.setTermioConfig();
}

static void sigAction(int sig, siginfo_t* siginfo, void* vptr) {

#ifdef DEBUG
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
#endif

    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            interrupted = true;
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

