/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-07-04 09:51:31 -0600 (Tue, 04 Jul 2006) $

    $LastChangedRevision: 3424 $

    $LastChangedBy: cjw $

    $HeadURL: http://localhost:8080/svn/nids/branches/nidas_reorg/src/nidas/apps/sensor_sim.cc $
 ********************************************************************

*/

#include <nidas/util/SerialPort.h>
#include <nidas/util/SerialOptions.h>

// #include <fcntl.h>
// #include <iostream>
// #include <cstring>

#include <vector>
#include <list>


using namespace std;

namespace n_u = nidas::util;

class TeeTTy {
public:
    TeeTTy();
    int parseRunstring(int argc, char** argv);
    int run();
    static int usage(const char* argv0);
private:
    string ttyname;
    n_u::SerialOptions ttyopts;
    list<string> rwptys;
    list<string> roptys;

    bool readonly;
};

TeeTTy::TeeTTy():readonly(true)
{
}

int TeeTTy::parseRunstring(int argc, char** argv)
{
    string ttyoptstr;
    int iarg = 1;
    if (iarg < argc) ttyname = argv[iarg++];
    if (iarg < argc) ttyoptstr = argv[iarg++];

    for ( ; iarg < argc; ) {
	string arg = argv[iarg];
	if (arg == "-w") {
	    if (++iarg == argc) usage(argv[0]);
	    rwptys.push_back(argv[iarg++]);	// user will read and write to/from this pty
	    readonly = false;
	}
	else roptys.push_back(argv[iarg++]);	// user will only read from this pty
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
Usage: " << argv0 << "tty ttyopts [ (-w ptyname) |  ptyname ] ... ]\n\
  tty: name of serial port to open\n\
  ttyopts: SerialOptions string, see below\n\
  -w ptyname: name of one or more read-write pseudo-terminals\n\
  ptyname: name of one or more read only pseudo-terminals\n\n\
  ttyopts:\n  " << n_u::SerialOptions::usage() << endl;
    return 1;
}

int TeeTTy::run()
{

    try {

	fd_set readfds;
	fd_set writefds;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);

	n_u::SerialPort tty(ttyname);
	tty.setOptions(ttyopts);

	tty.open(readonly ? O_RDONLY : O_RDWR);
	FD_SET(tty.getFd(),&readfds);
	int maxfd = tty.getFd() + 1;

	vector<int> ptyfds;
	vector<string> ptynames;

	list<string>::const_iterator li = rwptys.begin();
	for ( ; li != rwptys.end(); ++li) {
	    const string& name = *li;
	    int fd = n_u::SerialPort::createPtyLink(name);

#ifdef DO_NONBLOCK
	    if (::fcntl(fd,F_SETFL,O_NONBLOCK) < 0)
	        throw n_u::IOException(name,"fcntl F_SETFL,O_NONBLOCK",errno);
#endif
	    FD_SET(fd,&readfds);
	    FD_SET(fd,&writefds);
	    maxfd = std::max(maxfd,fd + 1);

	    ptyfds.push_back(fd);
	    ptynames.push_back(name);
	}
	li = roptys.begin();
	for ( ; li != roptys.end(); ++li) {
	    const string& name = *li;
	    int fd = n_u::SerialPort::createPtyLink(name);
#ifdef DO_NONBLOCK
	    if (::fcntl(fd,F_SETFL,O_NONBLOCK) < 0)
	        throw n_u::IOException(name,"fcntl F_SETFL,O_NONBLOCK",errno);
#endif
	    FD_SET(fd,&writefds);
	    maxfd = std::max(maxfd,fd + 1);

	    ptyfds.push_back(fd);
	    ptynames.push_back(name);
	}

	char buf[1024];
	for (;;) {

	    int nfd;
	    fd_set rfds = readfds;
	    fd_set wfds = writefds;
	    if ((nfd = ::select(maxfd,&rfds,&wfds,0,0)) < 0)
	    	throw n_u::IOException(tty.getName(),"select",errno);

	    if (FD_ISSET(tty.getFd(),&rfds)) {
		size_t l = tty.read(buf,sizeof(buf));
		if (l > 0) {
		    for (unsigned int i = 0; i < ptyfds.size(); i++)  {
			if (FD_ISSET(ptyfds[i],&wfds)) {
			    int lw = ::write(ptyfds[i],buf,l);
			    if (lw < 0) throw n_u::IOException(ptynames[i],"write",errno);
			    if (lw != (signed)l) cerr << ptynames[i] <<
				" wrote " << lw << " out of " << l << " bytes" << endl;
			}
		    }
		}
		FD_CLR(tty.getFd(),&rfds);
		nfd--;
	    }

	    for (int ifd = 0; nfd > 0 && ifd < maxfd; ifd++) {
		if (FD_ISSET(ifd,&rfds)) {
		    int l = ::read(ifd,buf,sizeof(buf));
		    if (l < 0) throw n_u::IOException(ptynames[ifd],"read",errno);
		    if (l > 0) tty.write(buf,l);
		    nfd--;
		}
	    }
	}
    }
    catch(n_u::IOException& ioe) {
	cerr << ioe.what() << endl;
	return 1;
    }
    return 0;
}
int main(int argc, char** argv)
{
    TeeTTy tee;
    int res;
    if ((res = tee.parseRunstring(argc,argv)) != 0) return res;
    tee.run();
}
