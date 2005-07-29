/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <DSMSerialSensor.h>

#include <atdUtil/Socket.h>

#include <iostream>
#include <sstream>

#include <poll.h>
#include <termios.h>
#include <signal.h>
#include <string.h>

using namespace std;

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

    void usage(const char* argv0);

    /**
     * Polling loop, reading and writing from stdin/stdout and socket.
     */
    void run() throw(atdUtil::IOException);

    /**
     * It's time to quit.
     */
    void interrupt() { interrupted = true; }

private:

    RemoteSerial();

    void removeStdin() { nfds = 1; }

    void setupSignals() throw(atdUtil::IOException);

    void setupStdin() throw(atdUtil::IOException);

    void restoreStdin() throw(atdUtil::IOException);

    void openConnection(atdUtil::Inet4SocketAddress saddr,
	const string& sensorName) throw(atdUtil::IOException);

    string socketReadLine() throw(atdUtil::IOException);

    enum output { HEX, ASCII, BOTH };

    void hexout(char c);

    static RemoteSerial* instance;

    bool interrupted;

    /**
     * by default print non-printing characters with back-slash escape
     * sequence tab=\t, etc.
     */
    bool escapeNonprinting;
		
    /**
     * indicates that a extra newline is to be added when outputting
     * each line to the terminal.
     */
    bool addNewLine;

    enum output binaryOutputOption;

    string hostName;

    unsigned short socketPort;

    bool stdinAltered;

    struct termios termio_save;

    atdUtil::Socket *socket;

    string sensorName;

    struct pollfd pollfds[2];

    int nfds;

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
};

RemoteSerial* RemoteSerial::instance = 0;

RemoteSerial::RemoteSerial(): interrupted(false),
    escapeNonprinting(true),addNewLine(false),binaryOutputOption(HEX),
    socketPort(8100),stdinAltered(false),socket(0),
    BUFSIZE(1024),buffer(new char[BUFSIZE]),
	bufhead(0),buftail(0)
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

    cerr << "received " << strsignal(isig) << " signal" << endl;

    switch (isig) {
    case SIGHUP:
    case SIGINT:
    case SIGTERM:
	rserial->interrupt();
	break;
    case SIGTTIN:
	// rserial will receive a SIGTTIN signal if it is put in 
	// the background and it tries to read from the controlling
	// terminal. Likewise it gets a SIGTTOU when it tries to write
	// (or do a tcsetattr) to the controlling terminal.
	// When we receive that signal, then remove stdin from the
	// poll.
	// Redirecting stdin from /dev/null is not a workaround, because
	// then rserial will get an immediate EOF and exit.
	rserial->removeStdin();
	break;
    case SIGTTOU:
	rserial->removeStdin();
	break;
    default:
	break;
    }
}

void RemoteSerial::setupSignals() throw(atdUtil::IOException)
{
    struct sigaction action;
    memset(&action,0,sizeof action);
    action.sa_handler = signalCatcher;
    if (sigaction(SIGINT,&action,0) < 0 ||
	sigaction(SIGTERM,&action,0) < 0 ||
	sigaction(SIGHUP,&action,0) < 0 ||
	sigaction(SIGTTIN,&action,0) < 0 ||
	sigaction(SIGTTOU,&action,0) < 0)

	throw atdUtil::Exception("sigaction",errno);
}

void RemoteSerial::hexout(char c) {
  switch (binaryOutputOption) {
  case HEX:	/* hex only */
    printf("%.2x ",(unsigned char) c);
    break;
  case BOTH:
    printf("%.2x",(unsigned char) c);
    if (isprint(c)) printf("'%c' ",c);
    else printf("    ");
    break;
  case ASCII:
    if (isprint(c) || isspace(c)) putc(c,stdout);
    else printf("\\%#.2x",c);
    break;
  }
}

void RemoteSerial::openConnection(atdUtil::Inet4SocketAddress saddr,
	const string& sensorName) throw(atdUtil::IOException)
{
    socket = new atdUtil::Socket(saddr);

    // send the device name
    string msg = sensorName + '\n';
    socket->sendall(msg.c_str(),msg.size());

    string line = socketReadLine();
    // cerr << "line=\"" << line << "\"" << endl;
    if (line.compare("OK"))
    	throw atdUtil::IOException("socket","read acknowledgement",line);

    line = socketReadLine();
    // cerr << "line=\"" << line << "\"" << endl;
    istringstream ist(line);
    ist >> baud;
    if (ist.fail())
    	throw atdUtil::IOException("socket","read baud rate",line);

    ist >> parity;
    if (ist.fail())
    	throw atdUtil::IOException("socket","read parity",line);

    ist >> databits >> stopbits;
    if (ist.fail())
    	throw atdUtil::IOException("socket","read databits",line);

    messageSeparator = socketReadLine();
    // cerr << "messageSeparator=" << messageSeparator << endl;

    line = socketReadLine();
    // cerr << "line=\"" << line << "\" length=" << line.size() << endl;
    ist.clear();
    ist.str(line);
    ist >> separatorAtEOM;
    if (ist.fail())
    	throw atdUtil::IOException("socket","read separator-at-eom",line);

    line = socketReadLine();
    // cerr << "line=\"" << line << "\"" << endl;
    ist.clear();
    ist.str(line);
    ist >> messageLength;
    if (ist.fail())
    	throw atdUtil::IOException("socket","read messageLength",line);

    cerr << "parameters: " << baud << ' ' << parity << ' ' <<
    	databits << ' ' << stopbits <<
	" \"" << messageSeparator << "\" " <<
	separatorAtEOM <<  ' ' << messageLength << endl;
    messageSeparator = dsm::DSMSerialSensor::replaceEscapeSequences(messageSeparator);
}

string RemoteSerial::socketReadLine() throw(atdUtil::IOException)
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

void RemoteSerial::setupStdin() throw(atdUtil::IOException)
{
    if (!isatty(0)) return;

    struct termios term_io_new;
    /* get the termio characterstics for stdin */
    if (tcgetattr(0,&termio_save) < 0) 
      throw atdUtil::IOException("stdin","tcgetattr",errno);

    /* copy termios settings and turn off local echo */
    memcpy( &term_io_new, &termio_save, sizeof termio_save);
    term_io_new.c_lflag &= ~ECHO & ~ICANON;

    term_io_new.c_iflag &= ~ICRNL & ~INLCR; 

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
    term_io_new.c_cc[VTIME] = 1;		// 1/10 second

    stdinAltered = true;

    if (tcsetattr(0,TCSAFLUSH,&term_io_new) < 0 && errno != EINTR)
	throw atdUtil::IOException("stdin","tcsetattr",errno);
}

void RemoteSerial::restoreStdin() throw(atdUtil::IOException)
{
    /* reset terminal to earlier state */
    if (!isatty(0) || !stdinAltered) return;

    if (tcsetattr(0,TCSAFLUSH,&termio_save) < 0 && errno != EINTR)
	throw atdUtil::IOException("stdin","tcsetattr",errno);
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
	atdUtil::Inet4SocketAddress saddr(
	    atdUtil::Inet4Address::getByName(hostName),socketPort);

	openConnection(saddr,sensorName);

	setupSignals();

	setupStdin();

	run();

	restoreStdin();
    }
    catch (const atdUtil::UnknownHostException& e) {
        cerr << e.toString() << endl;
	restoreStdin();
	return 1;
    }
    catch (const atdUtil::IOException& e) {
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

  while ((c = getopt(argc, argv, "neaA")) != EOF)
    switch (c) {
    case 'n':
      addNewLine = true;
      break;
    case 'e':
      escapeNonprinting = false;
      break;
    case 'a':
      binaryOutputOption = BOTH;
      break;
    case 'A':
      binaryOutputOption = ASCII;
      break;
    case '?':
      usage(argv[0]);
    }
  if (argc - optind < 3) usage(argv[0]);
  hostName = argv[optind++];

  istringstream ist(argv[optind++]);
  ist >> socketPort;
  if (ist.fail()) usage(argv[0]);

  sensorName = argv[optind++];
  return 0;

}

void RemoteSerial::usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << "\
[-n] [-e] [-A | -a] dsmName socketPort sensorName\n\
-n: add (extra) newline at end of each record\n\
-e: for non-binary sensors, don't print escape sequence for special characters\n\
-A: for binary sensors, print as ASCII, not hex\n\
-a: for binary sensors, print ASCII in addition to hex\n" << endl;
    exit (1);
}

void RemoteSerial::run() throw(atdUtil::IOException)
{

    pollfds[0].fd = socket->getFd();	// receive socket
    pollfds[0].events = POLLIN | POLLERR;
    pollfds[1].fd = 0;			// stdin
    pollfds[1].events = POLLIN | POLLERR;
    nfds = 2;

    const int POLLING_TIMEOUT = 300000;         // milliseconds

    int lsync = messageSeparator.size();
    int nsync = 0;
    int nout = 0;

    // the polling loop
    while (!interrupted) {
	int pollres,nread;
	switch (pollres = poll(pollfds,nfds,POLLING_TIMEOUT)) {
	case -1:
	    if (errno != EINTR)
	    	throw atdUtil::IOException("inputs","poll",errno);
	    continue;
	case 0:	// polling timeout
	    cerr << "Timeout waiting for input" << endl;
	    interrupt();
	    continue;
	default:
	    break;
	}
	if (pollfds[0].revents & POLLIN) {
	    try {
		nread = socket->recv(buffer, BUFSIZE);
	    }
	    catch (const atdUtil::EOFException& eof) {
	        cerr << "EOF on socket" << endl;
		nread = 0;
		interrupt();
	    }

#define PRINT_ESCAPE_SEQ
#ifdef PRINT_ESCAPE_SEQ
	    for (int ic = 0; ic < nread; ic++) {
		char c = buffer[ic];
		if (messageLength > 0) {		/* BINARY data */
		    if (nsync == lsync) {
			nout++;
			/*
			 * If read all chars in this record,
			 * set nsync to 0 to look for sync bytes next
			 */
			if (nout == messageLength) nsync = nout = 0;
			hexout(c);
		    }
		    else {
			/*
			 * checking against sync string.
			 */
			if (!separatorAtEOM) {
			    if (c != messageSeparator[nsync]) {
				for (int j = 0; j < nsync; j++)
				    hexout(messageSeparator[j]);
				if (c != messageSeparator[nsync = 0]) hexout(c);
				else nsync++;
			    }
			    else nsync++;

			    if (nsync == lsync) {
				printf("\r\n");
				for (nsync=0; nsync < lsync; nsync++)
				    hexout(messageSeparator[nsync]);
			    }
			    else {
				hexout(c);
				if (c == messageSeparator[nsync] ||
					    c == messageSeparator[nsync = 0])
				    nsync++;
				    if (nsync == lsync) printf("\r\n");
			    }
			}
		    }
		}
		else {
		    if (!escapeNonprinting) putc(c,stdout);
		    else {
			/* printf("%.2x ",(unsigned char) c); */

			if (isprint(c)) putc(c,stdout);
			else if (c == '\r') printf("\\r");
			else if (c == '\n') printf("\\n");
			else printf("\\%#x",c);
			if (c == messageSeparator[0]) printf("\r\n");
		    }
		}
	    }
#else
	    printf("%s", buffer);
	    if (n_flag) printf("\r\n");
#endif
	}
	if (pollfds[0].revents & POLLERR) {
	    throw atdUtil::IOException(
	    	socket->getInet4SocketAddress().toString(),"poll","POLLERR");
	    interrupt();
	}
	if (nfds > 1 && pollfds[1].revents & POLLIN) {	// data on stdin
	    // if (!isatty(0)) sleep(2);		// if reading from file
	    /* read a character from standard input */

	    if ((nread = read(0, buffer, sizeof buffer)) <= 0) {
		if (nread == 0) {
		    /* client read an EOF; exit the loop. */
		    interrupt();
		    break;
		}
		if (nread < 0 && errno != EINTR) {
		    throw atdUtil::IOException("stdin","read",errno);
		    interrupt();
		    break;
		}
	    }
	    if (nread == 1 && buffer[0] == '\004') {
		interrupt();
		break;
	    }
	    /* send bytes to adam */
	    if (nread > 0) socket->sendall(buffer, nread, 0);
	}
	if (nfds > 1 && pollfds[1].revents & POLLERR) {
	    throw atdUtil::IOException(
	    	socket->getInet4SocketAddress().toString(),"poll","POLLERR");
	    interrupt();
	}
    }
    printf("\r\n");
}

