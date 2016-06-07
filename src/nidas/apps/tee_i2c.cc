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
/*

    Simple program to "tee" the byte stream from an I2C device
    to one or more pseudo-terminals.

    TODO: As necessary, support other device protocols.

    Currently it is targeted at a ublox NEO 5Q GPS, that
    is attached to a Raspberry Pi2.

    The 5Q is obsolete, but this doc is available on the web:

    u-blox5_Protocol_Specifications(GPS.G5-X-07036).pdf

    Things work in a minimal, but perhaps sufficient, way.
    Can read the stream of NMEA packets from address 0xff.
    Reads of the length bytes at addresses 0xfd and 0xfe just
    return characters from the NMEA stream.
    Attempts at sending configure packets fail, such as trying
    to disable some NMEA messages.
    Have to put a usleep in the read loop, otherwise after several
    minutes, all I2C reads fail, with timeout errors until power cycle.
    Soft reboot doesn't fix it.
    This was a power cycle of both the RPI2 and the ublox.
    Don't know if we have the capability to power cycle only the
    ublox. Until we get writes to work, can't reset it by software.

    Need to test i2c_block_reads again, without reading the length
    bytes from 0xfd, 0xfe.
*/

#include <nidas/util/SerialPort.h>
#include <nidas/util/SerialOptions.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#include <linux/i2c-dev.h>
#include <linux/tty.h>

#include <vector>
#include <cstdlib>
#include <list>

#include <sched.h>
#include <signal.h>

// #define USE_SELECT

using namespace std;

namespace n_u = nidas::util;

bool interrupted = false;

/**
 * UBX configure packets.
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

/**
 * Attempt at sending configuration packets to a ublox NEO-5Q.
 * Doesn't work. Writes just return timeout errors, and no
 * acks are received back.  Not sure where the problem lies. 
 */
void ubx_config(int fd, const string& name) throw(n_u::IOException)
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

class TeeI2C {
public:
    TeeI2C();
    ~TeeI2C();

    int parseRunstring(int argc, char** argv);

    int run() throw();

    void i2c_byte_reads() throw(n_u::IOException);

    void i2c_block_reads() throw(n_u::IOException);

    void writeptys(const unsigned char* buf, int len) throw(n_u::IOException);

    void setFIFOPriority(int val);

    static int usage(const char* argv0);

#ifdef USE_SELECT
    void setupSignals() throw(n_u::IOException);
#endif


private:
    string progname;

    string _i2cname;

    unsigned int _i2caddr;

    int _i2cfd;

    vector<string> _ptynames;

    vector<int> _ptyfds;

    bool asDaemon;

    int priority;

    sigset_t _signalMask;

    fd_set _writefdset;

    int _maxwfd;

};

TeeI2C::TeeI2C():progname(),_i2cname(),_i2caddr(0), _i2cfd(-1),
    _ptynames(), _ptyfds(),
    asDaemon(true),priority(-1),_signalMask(), _writefdset(), _maxwfd(0)
{
}

TeeI2C::~TeeI2C()
{
    if (_i2cfd >= 0) close(_i2cfd);
    for (unsigned int i = 0; i < _ptynames.size(); i++) {
        if (_ptyfds[i] >= 0) ::close(_ptyfds[i]);
        ::unlink(_ptynames[i].c_str());
    }
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

#ifdef USE_SELECT
void TeeI2C::setupSignals() throw(n_u::IOException)
{

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
    sigaction(SIGUSR1,&act,(struct sigaction *)0);
#ifdef DO_SIGACTON
#endif

    // block HUP, TERM, INT, and unblock them in pselect
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGHUP);
    sigaddset(&sigs, SIGTERM);
    sigaddset(&sigs, SIGINT);
#ifdef USE_SIGPROCMASK
    if (sigprocmask(SIG_BLOCK, &sigs, &_signalMask) < 0)
        throw n_u::IOException("tee_i2c","sigprocmask",errno);
#else
    if (pthread_sigmask(SIG_BLOCK, &sigs, &_signalMask) < 0)
        throw n_u::IOException("tee_i2c","pthread_sigmask",errno);
#endif

    sigemptyset(&_signalMask);
    // sigdelset(&_signalMask,SIGHUP);
    // sigdelset(&_signalMask,SIGTERM);
    // sigdelset(&_signalMask,SIGINT);

}
#endif

int TeeI2C::parseRunstring(int argc, char** argv)
{
    progname = argv[0];
    int iarg = 1;

    for ( ; iarg < argc; iarg++) {
	string arg = argv[iarg];
	if (arg == "-f") asDaemon = false;	// don't put in background
	else if (arg == "-p") {
            if (++iarg == argc) return usage(argv[0]);
            {
                istringstream ist(argv[iarg]);
                ist >> priority;
                if (ist.fail()) return usage(argv[0]);
            }
        }
	else if (arg[0] == '-') return usage(argv[0]);
	else {
	    if (_i2cname.length() == 0) _i2cname = argv[iarg];
	    else if (_i2caddr == 0) _i2caddr = strtol(argv[iarg], NULL, 0);
	    else _ptynames.push_back(argv[iarg]);	// user will only read from this pty
	}
    }

    if (_i2cname.length() == 0) return usage(argv[0]);
    if (_i2caddr < 3 || _i2caddr > 255) {
        cerr << "i2caddr out of range" << endl;
        return usage(argv[0]);
    }
    if (_ptynames.empty()) return usage(argv[0]);


    return 0;
}

int TeeI2C::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-f] i2cdev i2caddr ptyname ... \n\
  -f: foreground. Don't run as background daemon\n\
  -p priority: set FIFO priority: 0-99, where 0 is low and 99 is highest.\n\
               If process lacks sufficient permissions,\n\
               a warning will be logged but " << argv0 << " will continue\n\
  i2cdev: name of I2C bus to open, e.g. /dev/i2c-1\n\
  i2caddr: address of I2C device, usually in hex: e.g. 0x42\n\
  ptyname: name of one or more read-only pseudo-terminals" << endl;
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
        nidas::util::Logger* logger = 0;
	n_u::LogConfig lc;
	n_u::LogScheme logscheme("tee_i2c");
	lc.level = 6;

	if (asDaemon) {
            if (daemon(0,0) < 0) throw n_u::IOException(progname,"daemon",errno);
            logger = n_u::Logger::createInstance(progname.c_str(),LOG_CONS,LOG_LOCAL5);
        }
        else logger = n_u::Logger::createInstance(&std::cerr);

        logscheme.addConfig(lc);
        logger->setScheme(logscheme);

        if (priority >= 0) setFIFOPriority(priority);

#ifdef USE_SELECT
        setupSignals();

	fd_set readfdset;
	FD_ZERO(&readfdset);
#endif
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

        _i2cfd = open(_i2cname.c_str(), O_RDWR);
        if (_i2cfd < 0)
            throw n_u::IOException(_i2cname, "open", errno);

        if (ioctl(_i2cfd, I2C_TIMEOUT, 30 * MSECS_PER_SEC / 10) < 0) {
            ostringstream ost;
            ost << "ioctl(,I2C_TIMEOUT,)";
            throw n_u::IOException(_i2cname, ost.str(), errno);
        }

        if (ioctl(_i2cfd, I2C_SLAVE, _i2caddr) < 0) {
            ostringstream ost;
            ost << "ioctl(,I2C_SLAVE," << hex << _i2caddr << ")";
            throw n_u::IOException(_i2cname, ost.str(), errno);
        }

        ubx_config(_i2cfd, _i2cname);

#ifdef USE_SELECT
	FD_SET(_i2cfd,&readfdset);
	int maxfd = _i2cfd + 1;
#endif

	for (interrupted = false; !interrupted; ) {

            /* Sleep a bit before next read
             * This seems to be important. Without it
             * reads eventually continually fail with timeout errors,
             * after several minutes; no data until a power cycle.
             * Posts on the net talk about ublox GPSs
             * "appling arbitrary clock stretches"
             * whatever that means.  Some folks have resorted to
             * doing direct bit-banging. 
             *
             * The i2c chip/driver on the RPi seems to be a minimal
             * implementation not really supporting blocking reads.
             */
            usleep(USECS_PER_SEC / 10);

#ifdef USE_SELECT
            /* this use of select is a leftover from tee_tty, which
             * reads from more than one device.
             */
	    int nfd;
	    fd_set rfds = readfdset;
	    if ((nfd = ::select(maxfd,&rfds,0,0,0)) < 0) {
                if (errno == EINTR) break;
	    	throw n_u::IOException(_i2cname,"select",errno);
            }

	    if (FD_ISSET(_i2cfd,&rfds)) {
		nfd--;
                // i2c_block_reads();
                i2c_byte_reads();
            }
#else
            i2c_byte_reads();
#endif
        }
    }
    catch(n_u::IOException& ioe) {
	PLOG(("%s",ioe.what()));
        result = 1;
    }
    return result;
}

void TeeI2C::i2c_block_reads() throw(n_u::IOException)
{
    unsigned char i2cbuf[I2C_SMBUS_I2C_BLOCK_MAX];

#ifdef READ_LEN_BYTES

    // Registers 0xfd and 0xfe don't seem to work as documented
    // for a ublox NEO 5Q. They contain '$' 'G', the first
    // two bytes in the NMEA stream.
    // Calling i2c_smbus_read_block_data on a ublox on a RPi2
    // locks up the system.

    // address 0xfd, number of bytes available, high byte
    int hb = i2c_smbus_read_byte_data(_i2cfd, 0xfd);
    if (hb < 0)
        throw n_u::IOException(_i2cname,"read",errno);
    if (hb == '\xff') return;
    // address 0xfe, number of bytes available, low byte
    int lb = i2c_smbus_read_byte_data(_i2cfd, 0xfe);
    if (lb < 0)
        throw n_u::IOException(_i2cname,"read",errno);
    if (lb == '\xff') return;
    int len = (hb & 0xff) << 8 | (lb & 0xff);

    // cerr << "len=" << len << endl;
#else
    int len = 512;  // arbitrary
#endif

    for ( ; len > 0; ) {

        // address 0xff, data stream
        int l = i2c_smbus_read_block_data(_i2cfd, 0xff, i2cbuf);
        if (l < 0)
            throw n_u::IOException(_i2cname,"read_block",errno);

        if (l == 0) break;
        // cerr << "l=" << l << endl;
        writeptys(i2cbuf,l);

        len -= l;
    }
}

void TeeI2C::i2c_byte_reads() throw(n_u::IOException)
{
    unsigned char i2cbuf[I2C_SMBUS_I2C_BLOCK_MAX];
    for ( ; ; ) {
        int len;
        for (len = 0; len < (signed) sizeof(i2cbuf); len++) {
            int db = i2c_smbus_read_byte_data(_i2cfd, 0xff);
#ifdef GIVE_UP
            if (db < 0) 
                throw n_u::IOException(_i2cname,"read_byte",errno);
#else
            if (db < 0) break;
#endif
            if (db == '\xff') break;
            i2cbuf[len] = (db & 0xff);
        }
        // cerr << "len=" << len << endl;
        if (len == 0) break;
        writeptys(i2cbuf,len);
    }
}

void TeeI2C::writeptys(const unsigned char* buf, int len)
    throw(n_u::IOException)
{
    int nwfd;
    fd_set wfds = _writefdset;

    struct timespec writeTimeout = {0,NSECS_PER_SEC / 10};

    /*
     * Only write to the ptys that are ready, so that
     * one can't block everybody.
     */
    if ((nwfd = ::pselect(_maxwfd,0,&wfds,0,&writeTimeout, &_signalMask)) < 0)
        throw n_u::IOException("ptys","pselect",errno);
    for (unsigned int i = 0; nwfd > 0 && i < _ptyfds.size(); i++)  {
        if (FD_ISSET(_ptyfds[i],&wfds)) {
            // cerr << _ptyfds[i] << " is writable, len=" << len << endl;
            nwfd--;
            for (unsigned int i = 0; i < _ptyfds.size(); i++)  {
                ssize_t lw = ::write(_ptyfds[i],buf, len);
                if (lw < 0) throw n_u::IOException(_ptynames[i],"write",errno);
            }
        }
    }
}

int main(int argc, char** argv)
{
    TeeI2C tee;
    int res;
    if ((res = tee.parseRunstring(argc,argv)) != 0) return res;

    return tee.run();
}

