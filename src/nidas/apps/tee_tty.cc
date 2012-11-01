// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    Simple program to "tee" I/O from a serial port to one
    or more pseudo-terminals.
 ********************************************************************

*/

#include <nidas/util/SerialPort.h>
#include <nidas/util/SerialOptions.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#ifdef HAS_TIMEPPS_H
#include <timepps.h>
#endif
#include <linux/tty.h>

#include <vector>
#include <list>

#include <sched.h>
#include <signal.h>

using namespace std;

namespace n_u = nidas::util;

bool interrupted = false;

class TeeTTy {
public:
    TeeTTy();
    int parseRunstring(int argc, char** argv);
    int run();
    void setFIFOPriority(int val);
    static int usage(const char* argv0);
    void setupSignals();
private:
    string progname;
    string ttyname;
    n_u::SerialOptions ttyopts;
    list<string> rwptys;
    list<string> roptys;

    bool readonly;
    bool asDaemon;

    int priority;

    sigset_t _signalMask;

    int linedisc;
    
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


TeeTTy::TeeTTy():progname(),ttyname(),ttyopts(),rwptys(),roptys(),
    readonly(true),asDaemon(true),priority(-1),_signalMask(),
    linedisc(-1)
{
}

void TeeTTy::setupSignals()
{
    // block HUP, TERM, INT, and unblock them in pselect
    sigemptyset(&_signalMask);
    sigaddset(&_signalMask,SIGHUP);
    sigaddset(&_signalMask,SIGTERM);
    sigaddset(&_signalMask,SIGINT);
    sigprocmask(SIG_BLOCK,&_signalMask,(sigset_t*)0);

    sigdelset(&_signalMask,SIGHUP);
    sigdelset(&_signalMask,SIGTERM);
    sigdelset(&_signalMask,SIGINT);

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);

}
int TeeTTy::parseRunstring(int argc, char** argv)
{
    progname = argv[0];
    string ttyoptstr;
    int iarg = 1;

    for ( ; iarg < argc; iarg++) {
	string arg = argv[iarg];
	if (arg == "-w") {
	    if (++iarg == argc) usage(argv[0]);
	    rwptys.push_back(argv[iarg]);	// user will read and write to/from this pty
	    readonly = false;
	}
	else if (arg == "-f") asDaemon = false;	// don't put in background
	else if (arg == "-p") {
            if (++iarg == argc) return usage(argv[0]);
            {
                istringstream ist(argv[iarg]);
                ist >> priority;
                if (ist.fail()) return usage(argv[0]);
            }
        }
	else if (arg == "-l") {
            if (++iarg == argc) return usage(argv[0]);
            istringstream ist (argv[iarg]);
            ist >> linedisc;
            if (ist.fail()) {
#ifdef LINUXPPS
                string ldstr(argv[iarg]);
                std::transform(ldstr.begin(), ldstr.end(),ldstr.begin(), ::toupper);
                if (ldstr != "PPS") return usage(argv[0]);
#ifdef N_PPS
                linedisc = N_PPS;
#else
                linedisc = 18;
#endif
#else
                return usage(argv[0]);
#endif
            }
        }
	else if (arg[0] == '-') return usage(argv[0]);
	else {
	    if (ttyname.length() == 0) ttyname = argv[iarg];
	    else if (ttyoptstr.length() == 0) ttyoptstr = argv[iarg];
	    else roptys.push_back(argv[iarg]);	// user will only read from this pty
	}
    }

    if (ttyname.length() == 0) return usage(argv[0]);
    if (ttyoptstr.length() == 0) return usage(argv[0]);
    if (rwptys.size() + roptys.size() == 0) return usage(argv[0]);

    try {
        ttyopts.parse(ttyoptstr);
    }
    catch (const n_u::ParseException &e) {
        cerr << e.what();
	return usage(argv[0]);
    }
    return 0;
}

int TeeTTy::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-f] tty ttyopts [ (-w ptyname) | ptyname ] ... ]\n\
  -f: foreground. Don't run as background daemon\n\
  -l ldisc:  Set the line discipline on the tty port. If a GPS is connected to\n\
             the port and is providing a pulse-per-second signal to the DCD line,\n\
             specify 18";
#ifdef LINUXPPS
    cerr << ", pps or PPS";
#endif
    cerr << " for ldisc.\n\
  -p priority: set FIFO priority: 0-99, where 0 is low and 99 is highest.\n\
               If process lacks sufficient permissions,\n\
               a warning will be logged but " << argv0 << " will continue\n\
  tty: name of serial port to open\n\
  ttyopts: SerialOptions string, see below\n\
  -w ptyname: name of one or more read-write pseudo-terminals\n\
  ptyname: name of one or more read-only pseudo-terminals\n\n\
  ttyopts:\n  " << n_u::SerialOptions::usage() << endl;
    return 1;
}

void TeeTTy::setFIFOPriority(int val)
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

int TeeTTy::run()
{
#ifdef DEBUG
    int nloop = 0;
    unsigned int maxread = 0;
    unsigned int minread = 99999999;
#endif

    setupSignals();

    vector<int> ptyfds;
    vector<string> ptynames;
    int result = 0;

    try {
	if (asDaemon) {
            if (daemon(0,0) < 0) throw n_u::IOException(progname,"daemon",errno);
            n_u::Logger::createInstance(progname.c_str(),LOG_CONS,LOG_LOCAL5);
        }
        else n_u::Logger::createInstance(&std::cerr);

        if (priority >= 0) setFIFOPriority(priority);

	fd_set readfds;
	fd_set writefds;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);

	n_u::SerialPort tty(ttyname);
        // transfer Termios
        tty.termios() = ttyopts.getTermios();
        bool raw = tty.termios().getRaw();

	tty.open(readonly ? O_RDONLY : O_RDWR);

        /* Attach the line discpline if requested. */
        if (linedisc >= 0 && ioctl(tty.getFd(), TIOCSETD, &linedisc) < 0)
	    	throw n_u::IOException(tty.getName(),"set line discipline: ioctl(,TIOCSETD,)",errno);

	FD_SET(tty.getFd(),&readfds);
	int maxfd = tty.getFd() + 1;
	int maxwfd = 0;

	struct timeval writeTimeout;

	list<string>::const_iterator li = rwptys.begin();
	for ( ; li != rwptys.end(); ++li) {
	    const string& name = *li;
	    int fd = n_u::SerialPort::createPtyLink(name);

            // Copy some attributes from the real serial port
            // to the pseudo-terminal. Currently only copying
            // the "raw" attribute
            n_u::Termios pterm(fd,name);
            pterm.setRaw(raw);
            pterm.apply(fd,name);

	    FD_SET(fd,&readfds);
	    FD_SET(fd,&writefds);
	    maxfd = std::max(maxfd,fd + 1);
	    maxwfd = std::max(maxwfd,fd + 1);

	    ptyfds.push_back(fd);
	    ptynames.push_back(name);
	}

	// user will only read from these ptys, so we only write to them.
	li = roptys.begin();
	for ( ; li != roptys.end(); ++li) {
	    const string& name = *li;
	    int fd = n_u::SerialPort::createPtyLink(name);

            // Copy some attributes from the real serial port
            // to the pseudo-terminal. Currently only copying
            // the "raw" attribute
            n_u::Termios pterm(fd,name);
            pterm.setRaw(raw);
            pterm.apply(fd,name);

	    FD_SET(fd,&writefds);
	    maxwfd = std::max(maxwfd,fd + 1);

	    ptyfds.push_back(fd);
	    ptynames.push_back(name);
	}

	char buf[1024];

	for (interrupted = false; !interrupted; ) {

	    int nfd;
	    fd_set rfds = readfds;
	    if ((nfd = ::pselect(maxfd,&rfds,0,0,0,&_signalMask)) < 0) {
                if (errno == EINTR) break;
	    	throw n_u::IOException(tty.getName(),"select",errno);
            }

	    if (FD_ISSET(tty.getFd(),&rfds)) {
		nfd--;
		int l = tty.read(buf,sizeof(buf));
#ifdef DEBUG
		if (l > maxread) maxread = l;
		if (l < minread) minread = l;
#endif
		if (l > 0) {
		    int nwfd;
		    fd_set wfds = writefds;
		    writeTimeout.tv_sec = 0;
		    writeTimeout.tv_usec = USECS_PER_SEC / 4;
		    if ((nwfd = ::select(maxwfd,0,&wfds,0,&writeTimeout)) < 0)
			throw n_u::IOException(tty.getName(),"select",errno);
		    for (unsigned int i = 0; nwfd > 0 && i < ptyfds.size(); i++)  {
			if (FD_ISSET(ptyfds[i],&wfds)) {
			    nwfd--;
			    ssize_t lw = ::write(ptyfds[i],buf,l);
			    if (lw < 0) throw n_u::IOException(ptynames[i],"write",errno);
			    if (lw != l) WLOG(("")  << ptynames[i] <<
				" wrote " << lw << " out of " << l << " bytes");
			}
		    }
		}
	    }

	    for (unsigned int i = 0; nfd > 0 && i < ptyfds.size(); i++)  {
		int fd = ptyfds[i];
		if (FD_ISSET(fd,&rfds)) {
		    nfd--;
		    ssize_t l = ::read(fd,buf,sizeof(buf));
		    if (l < 0) {
		        n_u::IOException e(ptynames[i],"read",errno);
                        PLOG(("%s",e.what()));
			::close(fd);
			ptyfds[i] = -1;
			FD_CLR(fd,&writefds);
			FD_CLR(fd,&readfds);

			// open a new one
			fd = n_u::SerialPort::createPtyLink(ptynames[i]);
			FD_SET(fd,&readfds);
			FD_SET(fd,&writefds);
			maxfd = std::max(maxfd,fd + 1);

			ptyfds[i] = fd;
		    }
		    if (l > 0) tty.write(buf,l);
		}
	    }
#ifdef DEBUG
	    if (!(nloop++  % 100))
	    	DLOG(("") << "nloop=" << nloop << " maxread=" << maxread << " minread=" << minread);
#endif
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
    TeeTTy tee;
    int res;
    if ((res = tee.parseRunstring(argc,argv)) != 0) return res;

    return tee.run();
}

