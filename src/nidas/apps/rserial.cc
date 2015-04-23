
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/dynld/DSMSerialSensor.h>

#include <nidas/util/Socket.h>

#include <cstdio>
#include <iostream>
#include <sstream>
#include <memory> // auto_ptr<>

#include <poll.h>
#include <termios.h>
#include <csignal>
#include <cstring>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

/**
 * RemoteSerial provides a connection between a user's stdin/stdout and
 * a DSMSerialSensor - so that one can send and receive
 * characters while the sensor is connected and being sampled by the
 * DSM.
 */
class RemoteSerial {

public:

    /**
     * Static method to get singleton instance. The singleton pattern
     * is used here so that signal handlers can access the RemoteSerial
     * instance.
     */
    static RemoteSerial* getInstance();

    /**
     * Public destructor.
     */
    ~RemoteSerial();

    /**
     * Signal handler.
     */
    static void signalCatcher(int isig);

    /**
     * Entry point for passing command line args.
     */
    int main(int argc, char *argv[]);

    /**
     * Configure parameters from runstring.
     */
    int parseRunstring(int argc, char *argv[]);

    int usage(const char* argv0);

    /**
     * Polling loop, reading and writing from stdin/stdout and socket.
     */
    void run() throw(n_u::IOException);

    /**
     * It's time to quit.
     */
    void interrupt() { interrupted = true; }

    static const char ESC = '\x1b';	// escape character

private:

    RemoteSerial();

    void removeStdin() {
        if (_stdinfd >= 0) close(_stdinfd);
        _stdinfd = -1;
    }

    void setupSignals() throw(n_u::IOException);

    void setupStdin() throw(n_u::IOException);

    void restoreStdin() throw(n_u::IOException);

    void openConnection(n_u::Inet4SocketAddress saddr,
	const string& sensorName) throw(n_u::IOException);

    string socketReadLine() throw(n_u::IOException);

    enum output { HEX, ASCII, BOTH };

    void charout(char c);

    static RemoteSerial* instance;

    bool interrupted;

    enum output outputOption;

    string hostName;

    unsigned short socketPort;

    bool stdinAltered;

    struct termios termio_save;

    n_u::Socket *socket;

    string sensorName;

    struct pollfd pollfds[2];

    const int BUFSIZE;

    char* buffer;

    int bufhead;

    int buftail;

    int baud;

    string parity;

    int databits;

    int stopbits;

    string messageSeparator;

    bool separatorAtEOM;

    int messageLength;

    int _stdinfd;

    /** No copying. */
    RemoteSerial(const RemoteSerial&);

    /** No assignment. */
    RemoteSerial& operator=(const RemoteSerial&);
};

RemoteSerial* RemoteSerial::instance = 0;

RemoteSerial::RemoteSerial(): interrupted(false),
    outputOption(ASCII),hostName("localhost"),socketPort(30002),
    stdinAltered(false),termio_save(),socket(0),sensorName(),
    BUFSIZE(1024),buffer(new char[BUFSIZE]),
    bufhead(0),buftail(0),baud(0),parity(),databits(0),stopbits(),
    messageSeparator(),separatorAtEOM(true),messageLength(0),
    _stdinfd(0)
{
    buffer[bufhead] = 0;
}

RemoteSerial::~RemoteSerial()
{
    if (socket) socket->close();
    delete socket;
    delete [] buffer;
    instance = 0;
}

RemoteSerial* RemoteSerial::getInstance()
{
    if (!instance) instance = new RemoteSerial();
    return instance;
}


/***************************************************************/
/**
 * catch signals
 */
void RemoteSerial::signalCatcher(int isig)
{
    RemoteSerial* rserial = RemoteSerial::getInstance();

    cerr << "received " << strsignal(isig) << " signal, isig=" << isig << endl;

    switch (isig) {
    case SIGHUP:
    case SIGINT:
    case SIGTERM:
	rserial->interrupt();
	break;
    case SIGTTIN:
    case SIGTTOU:
	// rserial will receive a SIGTTIN signal if it is put in 
	// the background and it tries to read from the controlling
	// terminal.  When we receive that signal, then remove stdin
        // from the poll.  Redirecting stdin from /dev/null when running
        // in the background is not a workaround, because
	// then rserial gets an immediate EOF and we then exit.
        //
        // Likewise it gets a SIGTTOU when it tries to write,
	// or do a tcsetattr on the controlling terminal.
        //
        // Even if we catch or ignore SIGTTIN and SIGTTOU the process
        // is still stopped. We'll give a hint about using the -n option.
        cerr << "To run in the background without reading from stdin, use the -n option" << endl;
	rserial->removeStdin();
	break;
    default:
	break;
    }
}

void RemoteSerial::setupSignals() throw(n_u::IOException)
{
    struct sigaction action;
    memset(&action,0,sizeof action);
    action.sa_handler = signalCatcher;
    if (sigaction(SIGINT,&action,0) < 0 ||
	sigaction(SIGTERM,&action,0) < 0 ||
	sigaction(SIGHUP,&action,0) < 0 ||
        sigaction(SIGTTIN,&action,0) < 0 ||
        sigaction(SIGTTOU,&action,0) < 0)
	throw n_u::Exception("sigaction",errno);

    // action.sa_handler = SIG_IGN;
}

void RemoteSerial::openConnection(n_u::Inet4SocketAddress saddr,
	const string& sensorName) throw(n_u::IOException)
{
    // if in background and no stdin, don't log these info messages

    if (_stdinfd >= 0) cerr << "connecting to " << saddr.toString() << endl;
    socket = new n_u::Socket(saddr);
    if (_stdinfd >= 0) cerr << "connected to " << saddr.toString() << endl;

    // send the device name
    string msg = sensorName + '\n';
    socket->sendall(msg.c_str(),msg.size());
    if (_stdinfd >= 0) cerr << "sent:\"" << msg << "\"" << endl;

    string line = socketReadLine();
    if (_stdinfd >= 0) cerr << "line=\"" << line << "\"" << endl;
    if (line.compare("OK"))
    	throw n_u::IOException("socket","read acknowledgement",line);

    line = socketReadLine();
    // cerr << "line=\"" << line << "\"" << endl;
    istringstream ist(line);
    ist >> baud;
    if (ist.fail())
    	throw n_u::IOException("socket","read baud rate",line);

    ist >> parity;
    if (ist.fail())
    	throw n_u::IOException("socket","read parity",line);

    ist >> databits >> stopbits;
    if (ist.fail())
    	throw n_u::IOException("socket","read databits",line);

    messageSeparator = socketReadLine();
    // cerr << "messageSeparator=" << messageSeparator << endl;

    line = socketReadLine();
    // cerr << "line=\"" << line << "\" length=" << line.size() << endl;
    ist.clear();
    ist.str(line);
    ist >> separatorAtEOM;
    if (ist.fail())
    	throw n_u::IOException("socket","read separator-at-eom",line);

    line = socketReadLine();
    // cerr << "line=\"" << line << "\"" << endl;
    ist.clear();
    ist.str(line);
    ist >> messageLength;
    if (ist.fail())
    	throw n_u::IOException("socket","read messageLength",line);

    string prompted = socketReadLine();

    if (_stdinfd >= 0) cerr << "parameters: " << baud << ' ' << parity << ' ' <<
    	databits << ' ' << stopbits <<
	" \"" << messageSeparator << "\" " <<
	separatorAtEOM <<  ' ' << messageLength << ' '<<
	prompted << endl;
    messageSeparator =
    	nidas::util::replaceBackslashSequences(messageSeparator);
}

string RemoteSerial::socketReadLine() throw(n_u::IOException)
{
    size_t rlen;      
    const char* nl;
    for (;;) {
	nl = ::strchr(buffer + buftail,'\n');
	if (nl) break;

	size_t len = BUFSIZE - bufhead - 1;     // length to read
	if (len == 0) {				// full buffer, no newline
	    nl = buffer + bufhead;		// fake it
	    break;
	}

	rlen = socket->recv(buffer + bufhead,len);
	bufhead += rlen;
	buffer[bufhead] = 0;			// null terminate
    }

    rlen = nl - buffer - buftail;
    string res = string(buffer + buftail,rlen);
    buftail += rlen + 1;		// skip newline
    // shift data down
    if (bufhead > buftail) {
        memmove(buffer,buffer + buftail,bufhead - buftail);
	bufhead -= buftail;
    }
    else bufhead = 0;
    buftail = 0;
    buffer[bufhead] = 0;			// null terminate
    return res;
}

void RemoteSerial::setupStdin() throw(n_u::IOException)
{
    if (_stdinfd < 0 || !isatty(_stdinfd)) return;

    struct termios term_io_new;
    /* get the termio characterstics for stdin */
    if (tcgetattr(_stdinfd,&termio_save) < 0) 
      throw n_u::IOException("stdin","tcgetattr",errno);

    /* copy termios settings and turn off local echo */
    memcpy( &term_io_new, &termio_save, sizeof termio_save);
    term_io_new.c_lflag &= ~ECHO & ~ICANON & ~TOSTOP;
    term_io_new.c_lflag |= ECHO;

    term_io_new.c_iflag &= ~INLCR & ~IGNCR; 
    // term_io_new.c_iflag |= ICRNL;		// input CR -> NL
    term_io_new.c_iflag &= ~ICRNL;		// input CR -> NL

    term_io_new.c_oflag &= ~OPOST;

    /* termio man page:
     Case A: MIN > 0, TIME > 0
	  In this case, TIME serves as  an  intercharacter  timer
	  and is activated after the first character is received.
	  Since it is an intercharacter timer, it is reset  after
	  a  character  is received.  The interaction between MIN
	  and TIME is as follows:  as soon as  one  character  is
	  received,  the intercharacter timer is started.  If MIN
	  characters are received before the intercharacter timer
	  expires  (note  that the timer is reset upon receipt of
	  each character), the read is satisfied.  If  the  timer
	  expires before MIN characters are received, the charac-
	  ters received to that point are returned to  the  user.
	  Note  that if TIME expires, at least one character will
	  be returned because  the  timer  would  not  have  been
	  enabled  unless a character was received.  In this case
	  (MIN > 0, TIME > 0), the read sleeps until the MIN  and
	  TIME  mechanisms  are  activated  by the receipt of the
	  first character.  If the number of characters  read  is
	  less than the number of characters available, the timer
	  is not reactivated and the subsequent read is satisfied
	  immediately.
    */
    term_io_new.c_cc[VMIN] = 1;
    term_io_new.c_cc[VTIME] = 5;		// 5/10 second

    stdinAltered = true;

    if (tcsetattr(_stdinfd,TCSAFLUSH,&term_io_new) < 0 && errno != EINTR)
	throw n_u::IOException("stdin","tcsetattr",errno);

    // turn off buffering of stdout
    if (setvbuf(stdout,0,_IONBF,0) != 0)
	throw n_u::IOException("stdout","setvbuf",errno);

}

void RemoteSerial::restoreStdin() throw(n_u::IOException)
{
    /* reset terminal to earlier state */
    if (_stdinfd < 0 || !isatty(_stdinfd) || !stdinAltered) return;

    if (tcsetattr(_stdinfd,TCSAFLUSH,&termio_save) < 0 && errno != EINTR)
	throw n_u::IOException("stdin","tcsetattr",errno);
}

int main(int argc, char *argv[])
{
  auto_ptr<RemoteSerial> rserial(RemoteSerial::getInstance());
  return rserial->main(argc,argv);
}

int RemoteSerial::main(int argc, char *argv[])
{

    int res = parseRunstring(argc,argv);
    if (res != 0) return res;

    try {
	n_u::Inet4SocketAddress saddr(
	    n_u::Inet4Address::getByName(hostName),socketPort);

	openConnection(saddr,sensorName);

	setupSignals();

	setupStdin();

	run();

	restoreStdin();
    }
    catch (const n_u::UnknownHostException& e) {
        cerr << e.toString() << endl;
	restoreStdin();
	return 1;
    }
    catch (const n_u::IOException& e) {
        cerr << e.toString() << endl;
	restoreStdin();
	return 1;
    }
    catch (...) {
        cerr << "unknown exception" << endl;
	restoreStdin();
	return 1;
    }
    return 0;
}

int RemoteSerial::parseRunstring(int argc, char *argv[])
{

  // extern char *optarg;
  extern int optind;
  int c;

  while ((c = getopt(argc, argv, "abhn")) != EOF)
    switch (c) {
    case 'a':
      outputOption = ASCII;
      break;
    case 'b':
      outputOption = BOTH;
      break;
    case 'h':
      outputOption = HEX;
      break;
    case 'n':
	removeStdin();
        break;
    case '?':
      return usage(argv[0]);
    }
  if (argc - optind < 1) return usage(argv[0]);
  sensorName = argv[optind++];

  if (argc - optind >= 1) {
      hostName = argv[optind++];
      string::size_type ci = hostName.find(':');
      if (ci != string::npos) {
          istringstream ist(hostName.substr(ci+1));
          ist >> socketPort;
          if (ist.fail()) return usage(argv[0]);
          hostName = hostName.substr(0,ci);
      }
  }

  return 0;

}

int RemoteSerial::usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << "\
[-a] [-h] [-b] sensor_device [dsm_host_name[:socketPort]]\n\
  sensor_device: name of device connected to the sensor, e.g. /dev/ttyS9\n\
      sensor_device should match the value of \"devicename\" in the configuration XML.\n\
      Typically it is a serial port name, but can also be the devicename of a\n\
      sensor connected to a TCP or UDP socket.\n\
      Note: rserial to UDP socket devices has a bug and needs some work.\n\
  dsm_host_name: host name of DSM. Defaults to \"localhost\"\n\
  socketPort: defaults to 30002 and typically doesn't need to be changed\n\
  -a: print sensor output as ASCII, not hex. -a is the default.\n\
  -h: print as hex\n\
  -b: print as both ASCII and hex\n\
  -n: don't read from stdin. This is required to avoid SIGTTIN and SIGTTOU signals\n\
      when running in the background if stdin is not redirected from the terminal\n\
Examples:\n\
    rserial /dev/ttyS9 dsm302\n\
    rserial /dev/ttyS9\n\
    rserial inet:psi9116:9000 dsm319\n\
\n\
Once rserial is connected you can use the following escape commands:\n\
ESC a   ASCII output\n\
ESC h   hex output\n\
ESC p	toggle sensor prompting\n\
ESC b rate	set baud rate\n\
ESC t sec	set timeout temporarily\n\
ESC T sec	set timeout permanently\n\
To terminate a connection, do ctrl-c or ctrl-d\n\
" << endl;
    return 1;
}

void RemoteSerial::run() throw(n_u::IOException)
{

    short events = POLLIN;

#ifdef POLLRDHUP
    events |= POLLRDHUP;
#endif

    pollfds[0].fd = socket->getFd();	// receive socket
    pollfds[0].events = events;
    pollfds[1].fd = _stdinfd;			// stdin
    pollfds[1].events = events;

    const int POLLING_TIMEOUT = 300000;         // milliseconds

    int lsync = messageSeparator.size();
    int nsync = 0;
    int nout = 0;
    bool lastCharEsc = false;
    bool nullTerm = false;
    if (separatorAtEOM && lsync > 0) {
        switch (messageSeparator[lsync-1]) {
	case '\r':
	case '\n':
	    nullTerm = true;
	default:;
	}
    }

    // the polling loop
    while (!interrupted) {
        int nfds = (_stdinfd < 0 ? 1 : 2);
	int pollres,nread;
	switch (pollres = poll(pollfds,nfds,POLLING_TIMEOUT)) {
	case -1:
	    if (errno != EINTR)
	    	throw n_u::IOException("inputs","poll",errno);
	    continue;
	case 0:	// polling timeout
	    cerr << "Timeout waiting for input" << endl;
	    interrupt();
	    continue;
	default:
	    break;
	}
	if (pollfds[0].revents & POLLIN) {	// data on socket
	    try {
		// cerr << "Reading " << BUFSIZE << " bytes" << endl;
		nread = socket->recv(buffer, BUFSIZE);
	    }
	    catch (const n_u::EOFException& eof) {
	        cerr << "EOF on socket" << endl;
		nread = 0;
		interrupt();
	    }

	    for (int ic = 0; ic < nread; ic++) {
		char c = buffer[ic];
		if (nullTerm && c == '\0' && nsync == 0) continue;
		if (messageLength > 0 && nsync == lsync) {
		    // matched record separator
		    nout++;
		    /*
		     * If read all chars in this record,
		     * set nsync to 0 to look for sync bytes next
		     */
		    if (nout == messageLength) nsync = 0;
		    charout(c);
		}
                else if (lsync == 0) charout(c);    // no separator
		else {
		    /*
		     * checking against sync string.
		     */
		    if (c != messageSeparator[nsync]) {
			for (int j = 0; j < nsync; j++)
			    charout(messageSeparator[j]);
			if (c != messageSeparator[nsync = 0]) charout(c);
			else nsync++;
		    }
		    else nsync++;

		    if (nsync == lsync) {
			if (!separatorAtEOM) printf("\r\n");
			for (nsync=0; nsync < lsync; nsync++)
			    charout(messageSeparator[nsync]);
			if (separatorAtEOM) printf("\r\n");
			nsync = 0;
			nout = 0;
		    }
		}
	    }
	}
	if (pollfds[0].revents & POLLERR) {
	    throw n_u::IOException(
	    	socket->getRemoteSocketAddress().toString(),"poll","POLLERR");
	    interrupt();
	}
	if (pollfds[0].revents & POLLHUP) {
	    interrupt();
            break;
	}
#ifdef POLLRDHUP
	if (pollfds[0].revents & POLLRDHUP) {
	    interrupt();
            break;
	}
#endif
	if (_stdinfd >= 0 && pollfds[1].revents & POLLIN) {	// data on stdin
	    /* read a character from standard input */

	    if ((nread = read(_stdinfd, buffer, BUFSIZE)) <= 0) {
		if (nread == 0) {
		    /* client read an EOF; exit the loop. */
		    interrupt();
		    break;
		}
		if (nread < 0) {
                    if (_stdinfd >= 0 && isatty(_stdinfd) && errno == EIO) {
                        cerr << n_u::IOException("stdin","read",errno).what() << endl;
                        removeStdin();
                    }
                    else throw n_u::IOException("stdin","read",errno);
		}
	    }
	    if (nread == 1 && buffer[0] == '\004') {
		interrupt();
		break;
	    }
	    int iout = 0;	// next character to send out
	    for (int i = 0; i < nread; i++) {
		if (lastCharEsc) {
		    switch (buffer[i]) {
		    case 'a':
			outputOption = ASCII;
			iout = i + 1;
			break;
		    case 'h':
			outputOption = HEX;
			iout = i + 1;
			break;
		    case ESC:
		    default:
			{
			    char tmp[2];
			    tmp[0] = ESC;
			    tmp[1] = buffer[i];
			    socket->sendall(tmp,2);
			}
			iout = i + 1;
			break;
		    }
		    // we send two escapes in a row
		    lastCharEsc = false;
		}
		else if (buffer[i] == ESC) {	// escape character
		    // send everything up to ESC
		    if (i > iout) socket->sendall(buffer+iout,i-iout);
		    iout = i + 1;
		    lastCharEsc = true;
		}
		else lastCharEsc = false;
	    }

	    /* send bytes to socket */
	    if (iout < nread) {
		// cerr << "sending " << nread-iout << " chars" << endl;
	        socket->sendall(buffer+iout, nread-iout);
	    }
	}
	if (_stdinfd >= 0 && pollfds[1].revents & POLLERR) {
	    throw n_u::IOException(
	    	socket->getRemoteSocketAddress().toString(),"poll","POLLERR");
	    interrupt();
	}
	if (_stdinfd >= 0 && pollfds[1].revents & POLLHUP) {
	    interrupt();
            break;
	}
#ifdef POLLRDHUP
	if (_stdinfd >= 0 && pollfds[1].revents & POLLRDHUP) {
	    interrupt();
            break;
	}
#endif
    }
    printf("\r\n");
}

void RemoteSerial::charout(char c)
{
    switch (outputOption) {
    case HEX:	/* hex only */
	printf("%02x ",(unsigned char) c);
	break;
    case BOTH:
	printf("%02x",(unsigned char) c);
	if (isprint(c)) printf("'%c' ",c);
	else printf("    ");
	break;
    case ASCII:
        switch(c) {
        case '\r':
            printf("\\r");
            break;
	case '\n':
            printf("\\n");
            break;
	case '\t':
            printf("\\t");
            break;
        default:
            if (isprint(c)) putc(c,stdout);
            else printf("\\%#04x",(unsigned char)c);
            break;
        }
    }
}
