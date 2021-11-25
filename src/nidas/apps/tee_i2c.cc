// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
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
/*
    Simple program to "tee" the byte stream from an I2C device
    to one or more pseudo-terminals.

    Currently it is targeted at a ublox NEO 5Q GPS, that
    is attached to a Raspberry Pi2.

    The 5Q is obsolete, but this doc is available on the web:

    u-blox5_Protocol_Specifications(GPS.G5-X-07036).pdf

    The I2C protocol used here does not provide blocking reads. The
    read system calls are implemented with ioctls(), which return
    any data available.  So the device must be polled. Currently the
    poll is done every 1/10 second.

    Our implementation of ublox 5/6 over I2C on an RPi suffers from data
    corruption. This may be related to the clock-stretching bug:
    https://www.advamation.com/knowhow/raspberrypi/rpi-i2c-bug.html,
    but this has not been verified.

    Also, for some reason on an RPi, the reads are subject to
    errors: EIO, EREMOTEIO.  Unless EXIT_ON_ERRORS is defined,
    just keep going, without throwing exceptions, logging the error
    or exiting. Perhaps this is related to the clock-stetching problem.

    This program also has code for the UBX protocol, the result of someone
    not RTFM. The ublox 5/6 do not support the binary UBX protocol over I2C,
    only RS232.  Only the ASCII UBX and NMEA messages are supported with I2C.
    The UBX protocol code has been left in for possible future RS232 
    implementation.

    The ublox program in nidas/apps sends UBX,40 messages to
    enable/disable individual NMEA messages.

    TODO: test i2c_block_reads again, without reading the length
    bytes from 0xfd, 0xfe.
*/

#include <nidas/util/SerialPort.h>
#include <nidas/util/SerialOptions.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>
#include <nidas/util/util.h>
#include <nidas/core/NidasApp.h>
#include <nidas/util/GPS.h>

#include <nidas/Config.h>

#ifdef HAVE_LINUX_I2C_DEV_H
#include <linux/i2c-dev.h>
#endif

#include <linux/tty.h>

#include <vector>
#include <cstdlib>
#include <list>
#include <iomanip>

#include <sched.h>
#include <signal.h>

using namespace std;
using nidas::core::NidasApp;
using nidas::core::NidasAppArg;
using nidas::core::ArgVector;
using nidas::core::NidasAppException;

namespace n_u = nidas::util;

bool interrupted = false;

// Whether to read the length values in registers 0xfd, 0xfe
// before reading the message stream in 0xff.
#define READ_LEN_REGS


class TeeI2C {
public:
    TeeI2C();
    ~TeeI2C();

    int parseRunstring(int argc, char** argv);

    int run() throw();

#ifdef HAVE_I2C_SMBUS_READ_BYTE
    void i2c_byte_reads();

    void i2c_block_reads();
#endif

    void writeOutput(const unsigned char* buf, int len);

    void setFIFOPriority(int val);

    int usage();

    /**
     * Check NMEA message for good checksum, date and time fields.
     * If OK, write to pseudoterminals.
     */
    void writeFilteredOutput(const char* input, int len, const fd_set& wfds);

    bool checkNMEA(const char* input, const char* eom);

    /**
     * Read NMEA messages from text file. Used to test the filter capability.
     */
    void readNMEAFile();

private:
    string progname;

    string _i2cname;

    unsigned int _i2caddr;

    int _i2cfd;

    vector<string> _ptynames;

    vector<bool> _filterGPS;

    bool _doFilterGPS;

    char _filterBuffer[4096];

    char* _fbhead, *_fbtail, *_fbend;

    vector<int> _ptyfds;

    bool asDaemon;

    int priority;

    sigset_t _signalMask;

    fd_set _writefdset;

    int _maxwfd;

    NidasApp _app;
    NidasAppArg Priority;
    NidasAppArg Foreground;
    NidasAppArg KeepMSB;
    NidasAppArg BlockingWrites;
    NidasAppArg OutputToFile;

    string _inputFile;

    string _filterOutputFileName;
    FILE* _outputFp;

    unsigned int _badChecksums;
    unsigned int _badFields;

    std::map<std::string, unsigned int> _badRMCFields;
    std::map<std::string, unsigned int> _badGGAFields;

    const int LOG_PERIOD_SECS = 3600;

    // Time of the last log message about filter statistics
    long long _lastLog;

    // No copying, assignment
    TeeI2C(const TeeI2C&);
    TeeI2C& operator=(const TeeI2C&);

};

TeeI2C::TeeI2C():
    progname(),_i2cname(),_i2caddr(0), _i2cfd(-1),
    _ptynames(), _filterGPS(), _doFilterGPS(false),
    _fbhead(_filterBuffer), _fbtail(_fbhead),
    _fbend(_fbhead + sizeof(_filterBuffer)),
    _ptyfds(),
    asDaemon(true),priority(-1),_signalMask(), _writefdset(), _maxwfd(0),
    _app(""),
    Priority("-p,--priority", "priority",
             "Set FIFO priority: 0-99, where 0 is low and 99 is highest.\n"
             "If process lacks sufficient permissions,\n"
             "a warning will be logged but program will continue.\n"
             "Note: since this just polls the I2C at a fairly slow rate,\n"
             "there isn't much to be gained by increasing the priority.\n"),
    Foreground("-f,--foreground", "", "Run foreground, not as background daemon"),
    KeepMSB("-k,--keepmsb", "",
            "By default the most-significant bit is cleared, to work\n"
            "around bugs on some Pi/Ublox.  Set this to leave the bit unchanged."),
    BlockingWrites("-b,--blocking", "",
                   "Use blocking writes instead of skipping writes to ptys which\n"
                   "are not immediately writable."),
    OutputToFile("-F,--filterout", "filename",
                   "Output filtered messages to file.\n"),
    _inputFile(), _filterOutputFileName(), _outputFp(0),
    _badChecksums(0), _badFields(0), _badRMCFields(), _badGGAFields(),
    _lastLog(n_u::UTime().toUsecs())
{
}

TeeI2C::~TeeI2C()
{
    if (_i2cfd >= 0) close(_i2cfd);
    for (unsigned int i = 0; i < _ptynames.size(); i++) {
        if (_ptyfds[i] >= 0) ::close(_ptyfds[i]);
        ::unlink(_ptynames[i].c_str());
    }
    if (_outputFp) ::fclose(_outputFp);
}

static void sigAction(int sig, siginfo_t* siginfo, void*)
{

    NLOG(("received signal ") << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1));


    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
    case SIGUSR1:
        interrupted = true;
    break;
    }
}

int TeeI2C::parseRunstring(int argc, char** argv)
{
    _app.enableArguments(_app.loggingArgs() | _app.Version | _app.Help |
                         Priority | Foreground | KeepMSB | BlockingWrites |
                         OutputToFile);

    ArgVector args = _app.parseArgs(argc, argv);

    if (_app.helpRequested())
        return usage();

    asDaemon = !Foreground.asBool();
    if (Priority.specified())
    {
        priority = Priority.asInt();
        if (priority < 0 || priority > 99)
        {
            throw NidasAppException("Priority must be 0-99");
        }
    }

    bool filterGPS = false;
    for (int iarg = 0; iarg < (signed)args.size(); iarg++) {
        string arg = args[iarg];
        if (arg == "-G") {
            filterGPS = true;
            _doFilterGPS = true;
        }
        else if (arg[0] == '-') return usage();
        else {
            if (_i2cname.length() == 0)
                _i2cname = arg;
            else if (_i2caddr == 0)
                _i2caddr = strtol(arg.c_str(), NULL, 0);
            else {
                // user will only read from this pty
                _ptynames.push_back(arg);
                _filterGPS.push_back(filterGPS);
                filterGPS = false;
            }
        }
    }

    if (_i2cname.length() == 0) return usage();
    if (_i2caddr == 0) {
        _inputFile = _i2cname;   // read from file
        _i2cname = "";
    }
    else if (_i2caddr < 3 || _i2caddr > 255) {
        cerr << "i2caddr out of range" << endl;
        return usage();
    }
#ifndef HAVE_I2C_SMBUS_READ_BYTE
    else {
        cerr << "i2c device I/O unsupported on this system" << endl;
        return usage();
    }
#endif

    _filterOutputFileName = OutputToFile.getValue();

    if (!_filterOutputFileName.empty()) _doFilterGPS = true;
    else if (_ptynames.empty()) return usage();

    return 0;
}

int TeeI2C::usage()
{
    cerr << "\
Usage: " << _app.getName() << "[-f] [-p priority] input i2caddr [-G] ptyname [-G] ptyname ...\n"
         << _app.usage() << 
#ifdef HAVE_I2C_SMBUS_READ_BYTE
"  input: name of I2C bus to open, e.g. /dev/i2c-1, or name of NMEA text file\n"
"  i2caddr: address of I2C device (or 0 if input from file), usually in hex: e.g. 0x42\n"
#else
"  input: name of NMEA text file\n"
"  i2caddr: 0 for input from file\n"
#endif
"  -G: messages to the following pty will be screened for invalid checksums and NMEA fields\n"
"  ptyname: name of a read-only pseudo-terminal\n\n"
"  Reading NMEA messages from a file is useful for testing the screen of invalid NMEA messasges.\n"
	<< endl;
    return 1;
}

void TeeI2C::setFIFOPriority(int val)
{
    struct sched_param sched;
    int pmin, pmax;

    pmax = sched_get_priority_max(SCHED_FIFO);
    pmin = sched_get_priority_min(SCHED_FIFO);
    sched.sched_priority = std::min(val,pmax);
    sched.sched_priority = std::max(val,pmin);
    if ( sched_setscheduler(0, SCHED_FIFO, &sched) == -1 ) {
        n_u::IOException e(progname,"set priority",errno);
        WLOG(("%s, continuing anyway",e.what()));
    }
}

int TeeI2C::run() throw()
{
    int result = 0;

    try {
        // NidasApp creates a log scheme and configures it, so use it.
        n_u::Logger* logger = n_u::Logger::getInstance();
        n_u::LogScheme logscheme = logger->getScheme();
    
        if (asDaemon) {
            if (daemon(0,0) < 0)
                throw n_u::IOException(progname, "daemon", errno);
            logger = n_u::Logger::createInstance(progname.c_str(),
                                                 LOG_CONS, LOG_LOCAL5);
        }
        else
            logger = n_u::Logger::createInstance(&std::cerr);

        logger->setScheme(logscheme);

        if (priority >= 0) setFIFOPriority(priority);

        FD_ZERO(&_writefdset);

        // user will only read from these ptys, so we only write to them.
        vector<string>::const_iterator li = _ptynames.begin();
        _maxwfd = 0;
        for ( ; li != _ptynames.end(); ++li) {
            const string& name = *li;
            int fd = n_u::SerialPort::createPtyLink(name);

            // set perms on slave device
            char slavename[PATH_MAX];
            if (ptsname_r(fd,slavename, sizeof(slavename)) < 0 ||
                chmod(slavename, 0444) < 0) {
                n_u::IOException e(name,"chmod",errno);
                WLOG(("")  << e.what());
            }

            n_u::Termios pterm(fd,name);
            pterm.setRaw(true);
            pterm.apply(fd,name);

            FD_SET(fd,&_writefdset);
            _maxwfd = std::max(_maxwfd,fd + 1);

            _ptyfds.push_back(fd);
        }

        if (!_filterOutputFileName.empty()) {
            int fd = ::open(_filterOutputFileName.c_str(),
                    O_CREAT | O_WRONLY | O_EXCL,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |
                    S_IROTH);
            if (fd < 0)
                throw n_u::IOException(_filterOutputFileName, "create", errno);
            if ((_outputFp = ::fdopen(fd, "w")) == NULL)
                throw n_u::IOException(_filterOutputFileName, "open", errno);
        }

        if (_i2cname.empty()) {
            readNMEAFile();
        }
#ifdef HAVE_I2C_SMBUS_READ_BYTE
        else {

            _i2cfd = open(_i2cname.c_str(), O_RDWR);
            if (_i2cfd < 0)
                throw n_u::IOException(_i2cname, "open", errno);

            if (ioctl(_i2cfd, I2C_TIMEOUT, 120 * MSECS_PER_SEC / 10) < 0) {
                ostringstream ost;
                ost << "ioctl(,I2C_TIMEOUT,)";
                throw n_u::IOException(_i2cname, ost.str(), errno);
            }

            if (ioctl(_i2cfd, I2C_SLAVE, _i2caddr) < 0) {
                ostringstream ost;
                ost << "ioctl(,I2C_SLAVE," << hex << _i2caddr << ")";
                throw n_u::IOException(_i2cname, ost.str(), errno);
            }

            for (interrupted = false; !interrupted; ) {

                /* Sleep a 1/10 second before next read.
                 */
                usleep(USECS_PER_SEC / 10);

                i2c_byte_reads();

                // block reads still don't work
                // i2c_block_reads();
            }
        }
#endif  // HAVE_I2C_SMBUS_READ_BYTE
    }
    catch(n_u::IOException& ioe) {
        PLOG(("%s",ioe.what()));
        result = 1;
    }
    return result;
}

void TeeI2C::readNMEAFile()
{
    unsigned char buf[1024];

    FILE* fp = ::fopen(_inputFile.c_str(),"r");

    if (fp == NULL) throw n_u::IOException(_inputFile,"open", errno);

    for ( ;; ) {
        size_t len = ::fread(buf, sizeof(*buf), sizeof(buf), fp);
        if (len == 0) {
            if (::feof(fp)) break;
            if (::ferror(fp)) {
                int ierr = errno;
                ::fclose(fp);
                throw n_u::IOException(_inputFile, "read", ierr);
            }
        }
        writeOutput(buf, len);
    }
    ::fclose(fp);
}

#ifdef HAVE_I2C_SMBUS_READ_BYTE
void TeeI2C::i2c_byte_reads()
{
    unsigned char i2cbuf[4096];
    // Unless KeepMSB is set, clear the high bit before writing to ptys.
    unsigned int mask = 0x7f;
    unsigned int msbfound = 0;

    if (KeepMSB.asBool())
        mask = 0xff;

    int lena = (signed)sizeof(i2cbuf)-1;

#ifdef READ_LEN_REGS
    // registers 0xfd, 0xfe are the (big-endian) number of bytes
    // in the ublox buffer.  After selecting register 0xfd,
    // it is auto-incremented on a read, and once it reaches
    // 0xff (the buffer), remains there.

    // number of bytes available, high byte
    int hb = i2c_smbus_read_byte_data(_i2cfd, 0xfd);


    if (hb < 0)
#ifdef EXIT_ON_IOERRORS
        throw n_u::IOException(_i2cname,"read",errno);
#else
        return;
#endif

    // ublox buffer is 4kB, high len byte can't be 0xff.
    // perhaps 0xff is an indication of no data available.
    if (hb == 0xff) return;

    // next address 0xfe, number of bytes available, low byte
    int lb = i2c_smbus_read_byte(_i2cfd);
    if (lb < 0)
#ifdef EXIT_ON_IOERRORS
        throw n_u::IOException(_i2cname,"read",errno);
#else
        return;
#endif

    lena = (hb & 0xff) << 8 | (lb & 0xff);

    if (lena == 0) return;
    // screen bad values
    if (lena < 0) lena = (signed)sizeof(i2cbuf)-1;
    else lena = std::min(lena, (signed)sizeof(i2cbuf)-1);
#endif

    int len;
    for (len = 0; len < lena; len++) {

        int db;
#ifndef READ_LEN_REGS
        if (len == 0)
            db = i2c_smbus_read_byte_data(_i2cfd, 0xff);
        else
#endif
            db = i2c_smbus_read_byte(_i2cfd);

#ifdef EXIT_ON_IOERRORS
        if (db < 0) 
            throw n_u::IOException(_i2cname,"read_byte",errno);
#else
        if (db < 0) break;
#endif
        if (db == 0xff) break;
        if (db > 0xff)
        {
            ++msbfound;
            VLOG(("i2c returned %u bytes > 0xff: %0x '%c' '%c'",
                  msbfound, db, (db >> 8) & 0x7f, db & 0x7f));
            if (msbfound % 100 == 1)
            {
                NLOG(("%d bytes > 0xff from i2c so far", msbfound));
            }
        }
        i2cbuf[len] = (db & mask);
    }
    // XXX what should happen if 0xff never found?  Do we really want
    // to dump 4096 bytes of unterminated i2c data to the ptys?
    // cerr << "len=" << len << endl;
    if (len > 0) {
        i2cbuf[len] = '\0';
        VLOG(("writing ") << len << " bytes to ptys: "
             << n_u::addBackslashSequences(string(i2cbuf, i2cbuf+len)));
        writeOutput(i2cbuf, len);
    }
}

void TeeI2C::i2c_block_reads()
{
    unsigned char i2cbuf[I2C_SMBUS_I2C_BLOCK_MAX + 2];
    int lena = (signed)sizeof(i2cbuf)-1;

#ifdef READ_LEN_REGS

    // address 0xfd, number of bytes available, high byte
    int hb = i2c_smbus_read_byte_data(_i2cfd, 0xfd);
    if (hb < 0)
#ifdef EXIT_ON_IOERRORS
        throw n_u::IOException(_i2cname,"read",errno);
#else
        return;
#endif

    // next address 0xfe, number of bytes available, low byte
    int lb = i2c_smbus_read_byte(_i2cfd);
    if (lb < 0)
#ifdef EXIT_ON_IOERRORS
        throw n_u::IOException(_i2cname,"read",errno);
#else
        return;
#endif

    lena = (hb & 0xff) << 8 | (lb & 0xff);

    if (lena == 0) return;

    // screen bad values
    if (lena < 0) lena = (signed)sizeof(i2cbuf)-1;
    else lena = std::min(lena, (signed)sizeof(i2cbuf)-1);

    // cerr << "lena=" << lena << endl;
#endif

// #define READ_BLOCK_DATA
#ifdef READ_BLOCK_DATA
    // This locks up the Pi.
    // address 0xff, data stream
    i2cbuf[0] = 32;
    int l = i2c_smbus_read_block_data(_i2cfd, 0xff, i2cbuf);
    if (l < 0)
        throw n_u::IOException(_i2cname,"read_block",errno);
    cerr << "block_reads, l=" << l << ", buf[0]=" << (int)i2cbuf[0] << endl;

    // if (l == 0) break;
#else
    // address 0xff, data stream
#ifdef OLD_I2C_API
    int l = i2c_smbus_read_i2c_block_data(_i2cfd, 0xff, i2cbuf);
#else
    int l = i2c_smbus_read_i2c_block_data(_i2cfd, 0xff, lena, i2cbuf);
#endif
    if (l < 0)
#ifdef EXIT_ON_IOERRORS
        throw n_u::IOException(_i2cname,"read_block",errno);
#else
        return;
#endif

    if (l == 0) return;
#endif

    int l2;
    for (l2 = 0; l2 < l && i2cbuf[l2] != 0xff; l2++);
    cerr << "block_reads, l=" << l << ", l2=" << l2 << endl;
    cerr << "buf= \"" << string((const char*)i2cbuf,l2) << "\"" << endl;
    writeOutput(i2cbuf,l2);
}
#endif  // HAVE_I2C_SMBUS_READ_BYTE

void TeeI2C::writeFilteredOutput(const char* buf, int len, const fd_set& wfds)
{
    /*
     * Check NMEA messages for good checksums and date,time fields.
     * If OK, write to those pty fds that are writeable.
     * This is complicated by the fact that the contents of buf
     * are not necessarily a single NMEA message.
     */
	
    while (len > 0) {

        // If input message is quite long (likely corrupt),
        // only handle a portion at a time.
        int len2 = std::min(len, (int) sizeof(_filterBuffer) / 4);

        // require room for one extra character so we can NULL terminate
        if (_fbtail + len2 + 1 > _fbend) {
            size_t n = _fbtail - _fbhead;
            if (n > sizeof(_filterBuffer) / 2) {
                // _filterBuffer is more than half full, probably due to 
                // corrupt messages, missing '$' and '\n' characters.
                // Look for last '$'. If found, and shifting contents will
                // make room for current buf, then shift down, else discard
                // all saved characters.
                const char* dp = (const char*) ::memrchr(_fbhead, '$', n);
                ssize_t discard = 0;
                if (dp) discard = dp - _fbhead;
                if (discard < len2) {
                    _fbhead = _fbtail = _filterBuffer;  // discard saved
                }
                else {
                    n = _fbtail - dp;
                    ::memmove(_filterBuffer, dp, n);
                    _fbhead = _filterBuffer;
                    _fbtail = _fbhead + n;
                }
            }
            else {
                ::memmove(_filterBuffer, _fbhead, n);
                _fbhead = _filterBuffer;
                _fbtail = _fbhead + n;
            }
        }
        // using less-than rather than less-or-equal since we add NULL
        assert(_fbtail + len2 < _fbend);

        // append buf to filterbuffer
        ::memcpy(_fbtail, buf, len2);
        _fbtail += len2;
        // NULL terminate since we're going to use sscanf.
        // Note it isn't necessarily NULL terminated right after
        // the '\n'.  The NULL may be in the middle of the next message.
        *_fbtail = 0;
        len -= len2;
        buf += len2;

        // check all GPS records in buffer
        for (;;) {
            if (_fbhead == _fbtail) break;
            char* p1 = _fbhead;
            // look for '$'
            if (*p1 != '$') p1 = (char*)::strchr(_fbhead, '$');
            if (!p1) {
                _fbhead = _fbtail = _filterBuffer;
                break;
            }
            // look for '\n'
            char* p2 = (char*)::strchr(p1, '\n');
            if (!p2) break;
            p2++;
            _fbhead = p2;

            bool badMessage = false;
            if (!nidas::util::NMEAchecksumOK(p1, p2-p1)) {
                _badChecksums++;
                badMessage = true;
            }
            else if (!checkNMEA(p1, p2)) {
                _badFields++;
                badMessage = true;
            }

            if (asDaemon) {
                long long now = n_u::UTime().toUsecs();
                float tdiff = (float)(now - _lastLog) / USECS_PER_SEC;
                if (tdiff > LOG_PERIOD_SECS) {
                    if (_badChecksums + _badFields > 0) {
                        float badperhour = (float)_badFields / tdiff * 3600.;
                        NLOG(("# bad checksums: ") << _badChecksums << " " <<
                                fixed << setprecision(2) <<
                                (float)_badChecksums / tdiff * 3600. << "/hr" <<
                                ", #bad fields: " << _badFields << " " <<
                                badperhour << "/hr");
                        // more verbose log message if many bad fields
                        // perhaps the GPS output format differs from what we expect
                        if (badperhour > 20) {
                            map<string, unsigned int>::const_iterator mi = _badRMCFields.begin();
                            ostringstream rmc;
                            for (int i = 0; mi != _badRMCFields.end(); ++mi,i++) {
                                if (i > 0) rmc << ", ";
                                if (mi->second > 0) rmc << mi->first << "=" << mi->second;
                            }
                            rmc << ends;
                            mi = _badGGAFields.begin();
                            ostringstream gga;
                            for (int i = 0; mi != _badGGAFields.end(); ++mi,i++) {
                                if (i > 0) gga << ", ";
                                if (mi->second > 0) gga << mi->first << "=" << mi->second;
                            }
                            gga << ends;
                            WLOG(("# bad RMC:") << rmc.str() << ", bad GGA:" << gga.str());
                        }
                    }
                    _lastLog = now;
                    _badChecksums = _badFields = 0;
                    map<string, unsigned int>::iterator mi = _badRMCFields.begin();
                    for ( ; mi != _badRMCFields.end(); ++mi) {
                        mi->second = 0;
                    }
                    mi = _badGGAFields.begin();
                    for ( ; mi != _badGGAFields.end(); ++mi) {
                        mi->second = 0;
                    }
                }
                if (badMessage) continue;
            }
            else if (badMessage) {
                const int NBAD_LOG = 1000;
                if ( ((_badChecksums + _badFields) % NBAD_LOG) == 1) {
                    ILOG(("# total bad checksums: ") << _badChecksums << " " <<
                            ", # total bad fields: " << _badFields);
                    map<string, unsigned int>::const_iterator mi = _badRMCFields.begin();
                    ostringstream rmc;
                    for (int i = 0; mi != _badRMCFields.end(); ++mi,i++) {
                        if (i > 0) rmc << ", ";
                        if (mi->second > 0) rmc << mi->first << "=" << mi->second;
                    }
                    rmc << ends;
                    mi = _badGGAFields.begin();
                    ostringstream gga;
                    for (int i = 0; mi != _badGGAFields.end(); ++mi,i++) {
                        if (i > 0) gga << ", ";
                        if (mi->second > 0) gga << mi->first << "=" << mi->second;
                    }
                    gga << ends;
                    ILOG(("# bad RMC:") << rmc.str() << ", bad GGA:" << gga.str());
                }
                continue;
            }

            for (unsigned int i = 0; i < _ptyfds.size(); i++)  {
                if (_filterGPS[i] && FD_ISSET(_ptyfds[i],&wfds)) {
                    ssize_t lw = ::write(_ptyfds[i], p1, p2 - p1);
                    if (lw < 0) throw n_u::IOException(_ptynames[i],"write",errno);
                }
            }
            if (_outputFp) {
                size_t lw = ::fwrite(p1, 1, p2 - p1, _outputFp);
                if (lw == 0 && ::ferror(_outputFp))
                    throw n_u::IOException(_filterOutputFileName, "write", errno);
            }
        }
        size_t n = _fbtail - _fbhead;
        if (n > 0) ::memmove(_filterBuffer, _fbhead, n);
        _fbhead = _filterBuffer;
        _fbtail = _fbhead + n;
    }
}

bool TeeI2C::checkNMEA(const char* input, const char* eom) 
{
    // since we use sscanf on input, it must be NULL terminated.
    // Therefore can also use strcmp instead of memcmp.
    bool RMC = !::strncmp(input,"$GPRMC,", 7);
    bool GGA = !RMC && !::strncmp(input,"$GPGGA,", 7);

    // pass this message on
    if (!RMC && !GGA) return true;

    input += 7;
    int hour, minute, second, nchar;
    const char* sof = input;
    bool badfield = false;
    if (sscanf(input,"%2d%2d%2d%n", &hour, &minute, &second, &nchar) != 3)
        badfield = true;
    else {
        int ncfsec = 0;
        if (nchar == 6 && input[6] == '.') {
            double fsec;
            sscanf(input+nchar,"%lf%n",&fsec,&ncfsec);
        }
        if (nchar != 6 ||
            hour < 0 || hour > 23 ||
            minute < 0 || minute > 59 ||
            second < 0 || second > 60) {
            badfield = true;
        }
        else {
            input += nchar + ncfsec;
            if (*input != ',') badfield = true;
        }
    }
    if (badfield) {
        if (RMC) {
            _badRMCFields["time"]++;
            VLOG(("RMC: bad time field: ") << string(sof,eom-sof));
        }
        else {
            _badGGAFields["time"]++;
            VLOG(("GGA: bad time field: ") << string(sof,eom-sof));
        }
        return false;
    }
    input ++;

    sof = input;
    if (RMC) {
        // Check receiver status, A=OK, V=warning, followed by comma
        if ((*input != 'A' && *input != 'V') || *++input != ',') {
            _badRMCFields["status"]++;
            VLOG(("RMC: bad status field: ") << string(sof,eom-sof));
            return false;
        }
        input++;
    }

    sof = input;
    // Latitude
    double f1, f2;
    if (sscanf(input,"%2lf%lf%n", &f1, &f2, &nchar) != 2)
        badfield = true;
    else {
        input += nchar;
        double val = f1 + f2 / 60.0;

        if (val < 0 || val > 91.0 || *input != ',')
            badfield = true;
        else if ((*++input != 'N' && *input != 'S') || *++input != ',')
            badfield = true;
    }
    if (badfield) {
        if (RMC) {
            _badRMCFields["lat"]++;
            VLOG(("RMC: bad lat field: ") << string(sof,eom-sof));
        }
        else {
            _badGGAFields["lat"]++;
            VLOG(("GGA: bad lat field: ") << string(sof,eom-sof));
        }
        return false;
    }
    input++;

    sof = input;
    // Longitude
    if (sscanf(input,"%3lf%lf%n", &f1, &f2, &nchar) != 2)
        badfield = true;
    else {
        input += nchar;
        double val = f1 + f2 / 60.0;
        if (val < 0 || val > 181.0 || *input != ',')
            badfield = true;
        else if ((*++input != 'E' && *input != 'W') || *++input != ',')
            badfield = true;
    }
    if (badfield) {
        if (RMC) {
            _badRMCFields["lon"]++;
            VLOG(("RMC: bad lon field: ") << string(sof,eom-sof));
        }
        else {
            _badGGAFields["lon"]++;
            VLOG(("GGA: bad lon field: ") << string(sof,eom-sof));
        }
        return false;
    }
    input++;

    if (RMC) {
        sof = input;
        // speed over ground, or perhaps empty field
        if (sscanf(input,"%lf%n", &f1, &nchar) == 1)
            input += nchar;
        if (*input != ',') {
            _badRMCFields["SOG"]++;
            VLOG(("RMC: bad SOG field: ") << string(sof,eom-sof));
            return false;
        }
        input++;

        sof = input;
        // course over ground, or perhaps empty field
        if (sscanf(input,"%lf%n", &f1, &nchar) == 1)
            input += nchar;
        if (*input != ',') {
            _badRMCFields["COG"]++;
            VLOG(("RMC: bad COG field: ") << string(sof,eom-sof));
            return false;
        }
        input++;

        sof = input;
        int day, month, year;
        if (sscanf(input,"%2d%2d%2d%n",&day,&month,&year,&nchar) != 3)
            badfield = true;
        else if (nchar != 6 || day < 1 || day > 31 ||
            month < 1 || month > 12 ||
            year < 19 || year > 30 || input[6] != ',') {
            badfield = true;
        }
        if (badfield) {
            _badRMCFields["date"]++;
            VLOG(("RMC: bad date field: ") << string(sof,eom-sof));
            return false;
        }
        input += nchar + 1;

        sof = input;
        // magnetic variation, E/W, or perhaps empty field
        if (sscanf(input,"%lf%n", &f1, &nchar) == 1)
            input += nchar;
        if (*input != ',' || (*++input != 'E' && *input != 'W' && *input != ',')) {
            _badRMCFields["magvar"]++;
            VLOG(("RMC: bad magvar field: ") << string(sof,eom-sof));
            return false;
        }
        input++;

        sof = input;
        if (*input != 'A' && *input != 'D' && *input != 'E' && *input != 'M' && *input != 'N') {
            _badRMCFields["mode"]++;
            VLOG(("RMC: bad mode field: ") << string(sof,eom-sof));
            return false;
        }
        input++;

        sof = input;
        if (*input != '*') {
            _badRMCFields["end"]++;
            VLOG(("RMC: bad end field: ") << string(sof,eom-sof));
            return false;
        }
    }
    else { // GGA message
        sof = input;
        // GGQUAL
        int ival;
        if (sscanf(input,"%d%n",&ival,&nchar) != 1 || input[nchar] != ',') {
            _badGGAFields["QUAL"]++;
            VLOG(("GGA: bad QUAL field: ") << string(sof,eom-sof));
            return false;
        }
        input += nchar + 1;

        sof = input;
        // GGNSAT
        if (sscanf(input,"%d%n",&ival,&nchar) != 1 || input[nchar] != ',') {
            _badGGAFields["NSAT"]++;
            VLOG(("GGA: bad NSAT field: ") << string(sof,eom-sof));
            return false;
        }
        input += nchar + 1;

        sof = input;
        // should be 6 commas left
        const char* cp;
        for (int i = 0; i < 6; i++) {
            cp = (const char*)::strchr(input, ',');
            if (!cp || cp > eom) {
                _badGGAFields["other"]++;
                VLOG(("GGA: bad other field: ") << string(sof,eom-sof));
                return false;
            }
            input = cp + 1;
        }

        sof = input;
        // diffential station ID
        cp = (const char*)::strchr(input, '*');
        if (!cp || cp > input + 4) {
            _badGGAFields["Did"]++;
            VLOG(("GGA: bad DID field: ") << string(sof,eom-sof));
            return false;
        }
    }
    return true;
}

void TeeI2C::writeOutput(const unsigned char* buf, int len)
{
    int nwfd = 0;
    fd_set wfds = _writefdset;
    bool blockwrites = BlockingWrites.asBool();
    struct timespec writeTimeout = {0,NSECS_PER_SEC / 10};

    /*
     * Either do non-blocking writes, or only write to
     * the ptys that are ready, so that one laggard,
     * such as a pseudo-terminal that isn't being read,
     * doesn't block everybody.
     */
    if (blockwrites) {
        nwfd = _ptyfds.size();
    }
    else if (_maxwfd > 0 && (nwfd = ::pselect(_maxwfd,0,&wfds,0,&writeTimeout, &_signalMask)) < 0)
        throw n_u::IOException("ptys","pselect",errno);

    if (_doFilterGPS) writeFilteredOutput((const char*)buf, len, wfds);

    for (unsigned int i = 0; i < _ptyfds.size(); i++)  {
        if (FD_ISSET(_ptyfds[i],&wfds)) {
            if (_filterGPS[i]) continue;
            if (blockwrites)
                VLOG(("pty ") << _ptyfds[i] << " blocking write of "
                     << len << " bytes.");
            else
                VLOG(("pty ") << _ptyfds[i] << " is writable, len=" << len);
            ssize_t lw = ::write(_ptyfds[i], buf, len);
            if (lw < 0) throw n_u::IOException(_ptynames[i],"write",errno);
            if (lw != len) WLOG(("")  << _ptynames[i] <<
                " wrote " << lw << " out of " << len << " bytes");
        }
        else
        {
            NLOG(("pty ") << _ptyfds[i] << " NOT writable, dropping "
                 << len << " bytes.");
        }
    }
}

int main(int argc, char** argv)
{
    TeeI2C tee;
    int res;
    try {
        if ((res = tee.parseRunstring(argc,argv)) != 0) return res;
    }
    catch (const NidasAppException &appx)
    {
        cerr << appx.what() << endl;
        cerr << "Use -h to see usage info." << endl;
        return 1;
    }
    return tee.run();
}

/**
 * UBX protocol configure packets, not supported in I2C.
 * This code is left here for possible future RS232 implementations.
 */
struct ubx_cfg_msg {
    unsigned char header[2];
    unsigned char cls;
    unsigned char id;
    unsigned short length;
    unsigned char payload[8];
    unsigned char cksum[2];
};

struct ubx_cfg_prt {
    unsigned char header[2];
    unsigned char cls;
    unsigned char id;
    unsigned short length;
    unsigned char payload[20];
    unsigned char cksum[2];
};

struct cfg_prt_payload {
    unsigned char portID;
    unsigned char res0[3];
    unsigned int mode;
    unsigned int res1;
    unsigned short inProtoMask;
    unsigned short outProtoMask;
    unsigned short flags;
    unsigned short res2;
};

void ubx_cksum(const unsigned char* cp, unsigned char* cksum)
{
    unsigned char ck_a = 0, ck_b = 0;
    for ( ; cp < cksum; cp++) {
        ck_a += *cp;
        ck_b += ck_a;
    }
    cksum[0] = ck_a;
    cksum[1] = ck_b;
}

#ifdef HAVE_I2C_SMBUS_READ_BYTE
/**
 * Attempt at sending configuration packets to a ublox NEO-5/6Q,
 * using the UBX protocol.  This doesn't work over I2C, only RS232.
 * With I2C, writes just return timeout errors, and no acks are
 * received back. Leave the code here for possible future RS232
 * implementations.
 */
void ubx_config(int fd, const string& name)
{
    unsigned char *mp;
    int wrres;

    struct ubx_cfg_prt pmsg;
    struct cfg_prt_payload ppay;
    pmsg.header[0] = 0xb5;
    pmsg.header[1] = 0x62;
    pmsg.cls = 0x06;
    pmsg.id = 0x00;
    pmsg.length = 20;
    memset(&ppay,0,sizeof(ppay));
    ppay.mode = 0x42;           // i2c address
    ppay.inProtoMask = 0x3;     // NMEA and UBX
    ppay.outProtoMask = 0x3;        // NMEA and UBX
    memcpy(pmsg.payload,&ppay,20);
    ubx_cksum(&pmsg.cls,&pmsg.cksum[0]);
    mp = (unsigned char*) &pmsg;

#define DO_SMBUS_WRITE
#ifdef DO_SMBUS_WRITE
    for ( ; mp <= &pmsg.cksum[1]; mp++) {
        wrres = i2c_smbus_write_byte(fd, *mp);
        if (wrres < 0) {
            n_u::IOException e(name,"write",errno);
            cerr << "Write error: " << e.what() << endl;
        }
    }
#else
    wrres = write(fd, mp, sizeof(pmsg));
    if (wrres < 0) {
        n_u::IOException e(name,"write",errno);
        cerr << "Write error: " << e.what() << endl;
    }
#endif
    for (int i = 0; i < 10; i++) {
        int db = i2c_smbus_read_byte(fd);
        if (db < 0) {
            n_u::IOException e(name,"read",errno);
            cerr << "read error: " << e.what() << endl;
        }
        cerr << "db=" << db << ' ' <<  hex << ((unsigned int)db & 0xff) << dec << endl;
    }

    struct ubx_cfg_msg msg;
    msg.header[0] = 0xb5;
    msg.header[1] = 0x62;
    msg.cls = 0x06;
    msg.id = 0x01;
    msg.length = 8;
    msg.payload[0] = 0xf0;
    msg.payload[1] = 0x05;  // turn off VTG
    for (int i = 2; i < 7; i++) msg.payload[i] = 0;
    msg.payload[7] = 1;

    ubx_cksum(&msg.cls,&msg.cksum[0]);
    cerr << "cksum=" << hex << (unsigned int) msg.cksum[0] <<
        ' ' << (unsigned int) msg.cksum[1] << dec << endl;

    mp = (unsigned char*) &msg;
#ifdef DO_SMBUS_WRITE
    for ( ; mp <= &msg.cksum[1]; mp++) {
        wrres = i2c_smbus_write_byte(fd, *mp);
        if (wrres < 0) {
            n_u::IOException e(name,"write",errno);
            cerr << "Write error: " << e.what() << endl;
        }
    }
#else
    wrres = write(fd, mp, sizeof(msg));
    if (wrres < 0) {
        n_u::IOException e(name,"write",errno);
        cerr << "Write error: " << e.what() << endl;
    }
#endif
    for (int i = 0; i < 10; i++) {
        int db = i2c_smbus_read_byte(fd);
        if (db < 0) {
            n_u::IOException e(name,"read",errno);
            cerr << "read error: " << e.what() << endl;
        }
        cerr << "db=" << db << ' ' <<  hex << ((unsigned int)db & 0xff) << dec << endl;
    }
}
#endif  // HAVE_I2C_SMBUS_READ_BYTE
