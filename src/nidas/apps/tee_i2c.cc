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

    Simple program to "tee" the byte stream read from an I2C device
    to one or more pseudo-terminals.

    TODO: As necessary, support other device protocols.
    Currently it is targeted at a ublox NEO 5Q GPS.

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

using namespace std;

namespace n_u = nidas::util;

bool interrupted = false;

class TeeI2C {
public:
    TeeI2C();
    ~TeeI2C();

    int parseRunstring(int argc, char** argv);

    int run() throw();

    void i2c_bytes() throw(n_u::IOException);

    void i2c_block() throw(n_u::IOException);

    void writeptys(const unsigned char* buf, int len) throw(n_u::IOException);

    void setFIFOPriority(int val);

    static int usage(const char* argv0);

    void setupSignals() throw(n_u::IOException);

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

TeeI2C::~TeeI2C(){
    if (_i2cfd >= 0) close(_i2cfd);
    for (unsigned int i = 0; i < _ptynames.size(); i++) {
        if (_ptyfds[i] >= 0) ::close(_ptyfds[i]);
        ::unlink(_ptynames[i].c_str());
    }
}

static void sigAction(int sig, siginfo_t* siginfo, void*) {

    NLOG(("received signal ") << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1));

    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
        interrupted = true;
    break;
    }
}

void TeeI2C::setupSignals() throw(n_u::IOException)
{
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);

    // block HUP, TERM, INT, and unblock them in pselect
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGHUP);
    sigaddset(&sigs, SIGTERM);
    sigaddset(&sigs, SIGINT);
    if (sigprocmask(SIG_BLOCK, &sigs, &_signalMask) < 0)
        throw n_u::IOException("tee_i2c","sigprocmask",errno);

    sigdelset(&_signalMask,SIGHUP);
    sigdelset(&_signalMask,SIGTERM);
    sigdelset(&_signalMask,SIGINT);

}
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

        setupSignals();

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_ZERO(&_writefdset);

        _i2cfd = open(_i2cname.c_str(), O_RDONLY);
        if (_i2cfd < 0)
            throw n_u::IOException(_i2cname, "open", errno);

        if (ioctl(_i2cfd, I2C_TIMEOUT, 5 * MSECS_PER_SEC / 10) < 0) {
            ostringstream ost;
            ost << "ioctl(,I2C_TIMEOUT,)";
            throw n_u::IOException(_i2cname, ost.str(), errno);
        }

        if (ioctl(_i2cfd, I2C_SLAVE, _i2caddr) < 0) {
            ostringstream ost;
            ost << "ioctl(,I2C_SLAVE," << hex << _i2caddr << ")";
            throw n_u::IOException(_i2cname, ost.str(), errno);
        }

	FD_SET(_i2cfd,&readfds);
	int maxfd = _i2cfd + 1;
	_maxwfd = 0;

	// user will only read from these ptys, so we only write to them.
	vector<string>::const_iterator li = _ptynames.begin();
	for ( ; li != _ptynames.end(); ++li) {
	    const string& name = *li;
	    int fd = n_u::SerialPort::createPtyLink(name);

            n_u::Termios pterm(fd,name);
            pterm.setRaw(true);
            pterm.apply(fd,name);

	    FD_SET(fd,&_writefdset);
	    _maxwfd = std::max(_maxwfd,fd + 1);

	    _ptyfds.push_back(fd);
	}

	for (interrupted = false; !interrupted; ) {

            // sleep a bit before next read
            // usleep(USECS_PER_SEC / 2);

	    int nfd;
	    fd_set rfds = readfds;
	    fd_set efds = readfds;
	    if ((nfd = ::pselect(maxfd,&rfds,0,&efds,0,&_signalMask)) < 0) {
                if (errno == EINTR) break;
	    	throw n_u::IOException(_i2cname,"select",errno);
            }
	    if (FD_ISSET(_i2cfd,&efds)) {
                cerr << "Exception in pselect" << endl;
            }

	    if (FD_ISSET(_i2cfd,&rfds)) {
		nfd--;
                // i2c_block();
                i2c_bytes();
            }
        }
    }
    catch(n_u::IOException& ioe) {
	PLOG(("%s",ioe.what()));
        result = 1;
    }
    return result;
}

void TeeI2C::i2c_block() throw(n_u::IOException)
{
    unsigned char i2cbuf[I2C_SMBUS_I2C_BLOCK_MAX];

    // Registers 0xfd and 0xfe don't seem to work as suspected
    // on a ublox NEO 5Q. They contain '$' 'G', the first
    // two bytes in the stream.
    // And calling i2c_smbus_read_block_data on a ublox on a RPi2
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

    cerr << "len=" << len << endl;

    for (int nzero = 0; len > 0 && nzero < 5; ) {

        // address 0xff, data stream
        int l = i2c_smbus_read_block_data(_i2cfd, 0xff, i2cbuf);
        if (l < 0)
            throw n_u::IOException(_i2cname,"read_block",errno);

        if (l == 0) {
            nzero++;
            usleep(USECS_PER_SEC / 10);
            continue;
        }
        cerr << "l=" << l << endl;
        writeptys(i2cbuf,l);

        len -= l;
    }
}

void TeeI2C::i2c_bytes() throw(n_u::IOException)
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
        if (len == 0) break;
        writeptys(i2cbuf,len);
    }
}

void TeeI2C::writeptys(const unsigned char* buf, int len)
    throw(n_u::IOException)
{
    int nwfd;
    fd_set wfds = _writefdset;

    struct timeval writeTimeout = {0,USECS_PER_SEC / 10};

    /*
     * Only write to which ever pty is ready, so that
     * one can't block everybody.
     */
    if ((nwfd = ::select(_maxwfd,0,&wfds,0,&writeTimeout)) < 0)
        throw n_u::IOException(_i2cname,"select",errno);
    for (unsigned int i = 0; nwfd > 0 && i < _ptyfds.size(); i++)  {
        if (FD_ISSET(_ptyfds[i],&wfds)) {
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

