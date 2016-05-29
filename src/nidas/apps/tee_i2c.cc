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

    Simple program to "tee" I/O from a serial port to one
    or more pseudo-terminals.

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
    void setFIFOPriority(int val);
    static int usage(const char* argv0);
    void setupSignals();
private:
    string progname;

    string i2cname;

    unsigned int i2caddr;

    int i2cfd;

    list<string> ptys;

    bool asDaemon;

    int priority;

    sigset_t _signalMask;

};

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


TeeI2C::TeeI2C():progname(),i2cname(),i2caddr(0), i2cfd(-1) ,ptys(),
    asDaemon(true),priority(-1),_signalMask()
{
}

TeeI2C::~TeeI2C(){
    if (i2cfd >= 0) close(i2cfd);
}

void TeeI2C::setupSignals()
{
    // block HUP, TERM, INT, and unblock them in pselect
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&_signalMask,SIGHUP);
    sigaddset(&_signalMask,SIGTERM);
    sigaddset(&_signalMask,SIGINT);
    if (sigprocmask(SIG_BLOCK,&sigs,&_signalMask) < 0) cerr << "Error in sigprocmask" << endl;

    sigdelset(&_signalMask,SIGHUP);
    sigdelset(&_signalMask,SIGTERM);
    sigdelset(&_signalMask,SIGINT);

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = sigAction;
    if (sigaction(SIGHUP,&act,(struct sigaction *)0) < 0) cerr << "Error in sigaction" << endl;
    if (sigaction(SIGINT,&act,(struct sigaction *)0) < 0) cerr << "Error in sigaction" << endl;
    if (sigaction(SIGTERM,&act,(struct sigaction *)0) < 0) cerr << "Error in sigaction" << endl;

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
	    if (i2cname.length() == 0) i2cname = argv[iarg];
	    else if (i2caddr == 0) i2caddr = strtol(argv[iarg], NULL, 0);
	    else ptys.push_back(argv[iarg]);	// user will only read from this pty
	}
    }

    if (i2cname.length() == 0) return usage(argv[0]);
    if (i2caddr < 3 || i2caddr > 255) {
        cerr << "i2caddr out of range" << endl;
        return usage(argv[0]);
    }
    if (ptys.empty()) return usage(argv[0]);


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

    setupSignals();

    vector<int> ptyfds;
    vector<string> ptynames;
    int result = 0;

    try {
        nidas::util::Logger* logger = 0;
	n_u::LogConfig lc;
	n_u::LogScheme logscheme("dsm");
	lc.level = 6;

	if (asDaemon) {
            if (daemon(0,0) < 0) throw n_u::IOException(progname,"daemon",errno);
            logger = n_u::Logger::createInstance(progname.c_str(),LOG_CONS,LOG_LOCAL5);
        }
        else logger = n_u::Logger::createInstance(&std::cerr);

        logscheme.addConfig(lc);
        logger->setScheme(logscheme);

        if (priority >= 0) setFIFOPriority(priority);

	fd_set readfds;
	fd_set writefds;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);

        i2cfd = open(i2cname.c_str(), O_RDONLY);
        if (i2cfd < 0)
            throw n_u::IOException(i2cname, "open", errno);

        if (ioctl(i2cfd, I2C_SLAVE, i2caddr) < 0) {
            ostringstream ost;
            ost << "ioctl(,I2C_SLAVE," << hex << i2caddr << ")";
            throw n_u::IOException(i2cname, ost.str(), errno);
        }

	FD_SET(i2cfd,&readfds);
	int maxfd = i2cfd + 1;
	int maxwfd = 0;

	struct timeval writeTimeout;

	// user will only read from these ptys, so we only write to them.
	list<string>::const_iterator li = ptys.begin();
	for ( ; li != ptys.end(); ++li) {
	    const string& name = *li;
	    int fd = n_u::SerialPort::createPtyLink(name);

            n_u::Termios pterm(fd,name);
            pterm.setRaw(true);
            pterm.apply(fd,name);

	    FD_SET(fd,&writefds);
	    maxwfd = std::max(maxwfd,fd + 1);

	    ptyfds.push_back(fd);
	    ptynames.push_back(name);
	}

	for (interrupted = false; !interrupted; ) {

	    int nfd;
	    fd_set rfds = readfds;
	    if ((nfd = ::pselect(maxfd,&rfds,0,0,0,&_signalMask)) < 0) {
                if (errno == EINTR) break;
	    	throw n_u::IOException(i2cname,"select",errno);
            }

	    if (FD_ISSET(i2cfd,&rfds)) {
		nfd--;
                // address 0xfd, number of bytes available, high byte
		int hb = i2c_smbus_read_byte_data(i2cfd, 0xfd);
                if (hb < 0)
                    throw n_u::IOException(i2cname,"read",errno);
                // address 0xfe, number of bytes available, low byte
		int lb = i2c_smbus_read_byte_data(i2cfd, 0xfe);
                if (lb < 0)
                    throw n_u::IOException(i2cname,"read",errno);
                int len = (hb & 0xff) << 8 | (lb & 0xff);

                for (int nzero = 0; len > 0 && nzero < 5; ) {
                    unsigned char i2cbuf[32];
                    // address 0xff, data stream
                    int l = i2c_smbus_read_block_data(i2cfd, 0xff, i2cbuf);
                    if (l < 0)
                        throw n_u::IOException(i2cname,"select",errno);

                    if (l == 0) {
                        nzero++;
                        usleep(USECS_PER_SEC / 10);
                    }
                    else {
                        int nwfd;
                        fd_set wfds = writefds;
                        writeTimeout.tv_sec = 0;
                        writeTimeout.tv_usec = USECS_PER_SEC / 10;
                        /*
                         * Only write to whomever is ready, so that
                         * one can't block everybody.
                         */
                        if ((nwfd = ::select(maxwfd,0,&wfds,0,&writeTimeout)) < 0)
                            throw n_u::IOException(i2cname,"select",errno);
                        for (unsigned int i = 0; nwfd > 0 && i < ptyfds.size(); i++)  {
                            if (FD_ISSET(ptyfds[i],&wfds)) {
                                nwfd--;
                                for (unsigned int i = 0; i < ptyfds.size(); i++)  {
                                    ssize_t lw = ::write(ptyfds[i],i2cbuf,l);
                                    if (lw < 0) throw n_u::IOException(ptynames[i],"write",errno);
                                }
                            }
                        }
                    }
                    len -= l;
                }
	    }

            // sleep a bit before next read
            usleep(USECS_PER_SEC / 10);
	}
    }
    catch(n_u::IOException& ioe) {
	PLOG(("%s",ioe.what()));
        result = 1;
    }
    for (unsigned int i = 0; i < ptynames.size(); i++) {
        if (ptyfds[i] >= 0) ::close(ptyfds[i]);
        ::unlink(ptynames[i].c_str());
    }
    return result;
}

int main(int argc, char** argv)
{
    TeeI2C tee;
    int res;
    if ((res = tee.parseRunstring(argc,argv)) != 0) return res;

    return tee.run();
}

