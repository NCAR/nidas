//              Copyright (C) by UCAR
//
// Description:
//


#include <nidas/util/SerialPort.h>
#include <sys/ioctl.h>
#include <sys/param.h>	// MAXPATHLEN
#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <sstream>

using namespace std;
using namespace nidas::util;

SerialPort::SerialPort(const string& name) : Termios(),
	_fd(-1),_name(name),_state(OK),_savebuf(0),_savelen(0),
	_savealloc(0),blocking(true)
{
}

SerialPort::SerialPort() : Termios(),
	_fd(-1),_name("/dev/unknown"),
	_state(OK),_savebuf(0),_savelen(0),
	_savealloc(0),blocking(true)
{
}

SerialPort::SerialPort(const string& name, int fd) throw(IOException) :
	Termios(),_fd(-1),_name(name),_state(OK),_savebuf(0),_savelen(0),
	_savealloc(0),blocking(true)
{
    _fd = fd;
    getTermioConfig();
    getBlocking();
}

SerialPort::SerialPort(const SerialPort& x) : Termios(x),
	_fd(0),_name(x._name),
	_state(OK),_savebuf(0),_savelen(0),
	_savealloc(0),blocking(x.blocking)
{
}

SerialPort::~SerialPort()
{
    close();
    delete [] _savebuf;
}

void
SerialPort::close() {
    if (_fd >= 0) {
	::close(_fd);
	cerr << "Closing: " << getName() << endl;
    }
    _fd = -1;
}

int
SerialPort::open(int mode) throw(IOException) 
{
  cerr << "Opening: " << getName() << endl;

  if ((_fd = ::open(_name.c_str(),mode)) < 0)
    throw IOException(_name,"open",errno);
  setTermioConfig();
  setBlocking(blocking);
  return _fd;
}

void
SerialPort::getTermioConfig() throw(IOException)
{
  getTermios(_fd,_name);
}

void
SerialPort::setTermioConfig() throw(IOException)
{
  setTermios(_fd,_name);
}

void
SerialPort::setBlocking(bool val) throw(IOException)
{
  if (_fd < 0) {
    blocking = val;
    return;
  }
  int flags;
  if ((flags = fcntl(_fd,F_GETFL)) < 0)
    throw IOException(_name,"fcntl F_GETFL",errno);

  if (val) flags &= ~O_NONBLOCK;
  else flags |= O_NONBLOCK;

  if (fcntl(_fd,F_SETFL,flags) < 0)
    throw IOException(_name,"fcntl F_SETFL",errno);
  blocking = val;
}

bool
SerialPort::getBlocking() throw(IOException) {
  if (_fd < 0) return blocking;

  int flags;
  if ((flags = fcntl(_fd,F_GETFL)) < 0)
    throw IOException(_name,"fcntl F_GETFL",errno);

  blocking = (flags & O_NONBLOCK) == 0;
  return blocking;
}

int
SerialPort::getModemStatus() throw(IOException)
{
    int modem=0;
    if (::ioctl(_fd, TIOCMGET, &modem) < 0)
      throw IOException(_name,"ioctl TIOCMGET",errno);
    return modem;
}

void
SerialPort::setModemStatus(int val) throw(IOException)
{
    if (::ioctl(_fd, TIOCMSET, &val) < 0)
      throw IOException(_name,"ioctl TIOCMSET",errno);
}

void
SerialPort::clearModemBits(int bits) throw(IOException)
{
    if (::ioctl(_fd, TIOCMBIC, &bits) < 0)
      throw IOException(_name,"ioctl TIOCMBIC",errno);
}

void
SerialPort::setModemBits(int bits) throw(IOException)
{
    if (::ioctl(_fd, TIOCMBIS, &bits) < 0)
      throw IOException(_name,"ioctl TIOCMBIS",errno);
}

bool
SerialPort::getCarrierDetect() throw(IOException)
{
    return (getModemStatus() & TIOCM_CAR) != 0;
}

string
SerialPort::modemFlagsToString(int modem)
{
  string res;

  static const char *offon[]={"OFF","ON"};
  static int status[] = {
    TIOCM_LE, TIOCM_DTR, TIOCM_RTS, TIOCM_ST, TIOCM_SR,
    TIOCM_CTS, TIOCM_CAR, TIOCM_RNG, TIOCM_DSR};
  static const char *lines[] =
    {"LE","DTR","RTS","ST","SR","CTS","CD","RNG","DSR"};

  for (unsigned int i = 0; i < sizeof status / sizeof(int); i++) {
    res += lines[i];
    res += '=';
    res += offon[(modem & status[i]) != 0];
    res += ' ';
  }
  return res;
}

int
SerialPort::flushOutput() throw(IOException)
{
  int r;
  if ((r = tcflush(_fd,TCOFLUSH)) < 0)
    throw IOException(_name,"tcflush TCOFLUSH",errno);
  return r;
}

int
SerialPort::flushInput() throw(IOException)
{
  int r;
  if ((r = tcflush(_fd,TCIFLUSH)) < 0)
    throw IOException(_name,"tcflush TCIFLUSH",errno);
  return r;
}

int
SerialPort::flushBoth() throw(IOException)
{
  int r;
  if ((r = tcflush(_fd,TCIOFLUSH)) < 0)
    throw IOException(_name,"tcflush TCIOFLUSH",errno);
  return r;
}

int
SerialPort::readUntil(char *buf, int len,char term) throw(IOException)
{
    len--;		// allow for trailing null
    int toread = len;
    int rd,i,l;

    // check for data left from last read
    if (_savelen > 0) {

	l = toread < _savelen ? toread : _savelen;
// #define DEBUG
#ifdef DEBUG
	cerr << "_savelen=" << _savelen << " l=" << l << endl;
#endif
	for (i = 0; i < l; i++) {
	    toread--;_savelen--;
	    if ((*buf++ = *_savep++) == term) break;
	}
	if (i < l) {	// term found
	    *buf = '\0';
	    return len - toread;
	}
#ifdef DEBUG
	cerr << "_savelen=" << _savelen << " l=" << l << " i=" << i << endl;
#endif
    }

    while (toread > 0) {
	switch(rd = read(buf,toread)) {
	case 0:		// EOD or timeout, user must figure out which
	    *buf = '\0';
	    return len - toread;
	default:
	    for (; rd > 0;) {
		rd--;
		toread--;
#ifdef DEBUG
		cerr << "buf char=" << hex << (int)(unsigned char) *buf <<
			" term=" << (int)(unsigned char) term << dec << endl;
#endif
		if (*buf++ == term) {
		  // save chars after term
		    if (rd > 0) {
			if (rd > _savealloc) {
			    delete [] _savebuf;
			    _savebuf = new char[rd];
			    _savealloc = rd;
			}
			::memcpy(_savebuf,buf,rd);
			_savep = _savebuf;
			_savelen = rd;
		    }
		    *buf = '\0';
		    return len - toread;
		}
	    }
#ifdef DEBUG
	    cerr << "rd=" << rd << " toread=" << toread << " _savelen=" << _savelen << endl;
#endif
	    break;
	}
    }
    *buf = '\0';
    return len - toread;
}

int
SerialPort::readLine(char *buf, int len) throw(IOException)
{
    return readUntil(buf,len,'\n');
}

int
SerialPort::write(const void *buf, int len) throw(IOException)
{
    if ((len = ::write(_fd,buf,len)) < 0)
      throw IOException(_name,"write",errno);
    return len;
}

int
SerialPort::read(char *buf, int len) throw(IOException)
{
    if ((len = ::read(_fd,buf,len)) < 0)
      throw IOException(_name,"read",errno);
    // set the state for buffered read methods
    _state = (len == 0) ? TIMEOUT_OR_EOF : OK;
#ifdef DEBUG
    cerr << "SerialPort::read len=" << len << endl;
#endif
    return len;
}

/**
 * Do a buffered read and return character read.
 * If '\0' is read, then do a check of timeoutOrEOF()
 * to see if the basic read returned 0.
 */
char
SerialPort::readchar() throw(IOException)
{
    if (_savelen == 0) {
	if (_savealloc == 0) {
	    delete [] _savebuf;
	    _savealloc = 512;
	    _savebuf = new char[_savealloc];
	}

	switch(_savelen = read(_savebuf,_savealloc)) {
	    case 0:
	      return '\0';
	    default:
	      _savep = _savebuf;
	}
    }
    _savelen--;
    return *_savep++;
}

/* static */
int SerialPort::createPtyLink(const std::string& link) throw(IOException)
{
    int fd;
    const char* ptmx = "/dev/ptmx";

    // could also use getpt() here.
    if ((fd = ::open(ptmx,O_RDWR|O_NOCTTY)) < 0) 
    	throw IOException(ptmx,"open",errno);

    char* slave = ptsname(fd);
    if (!slave) throw IOException(ptmx,"ptsname",errno);

    // cerr << "slave pty=" << slave << endl;

    if (grantpt(fd) < 0) throw IOException(ptmx,"grantpt",errno);
    if (unlockpt(fd) < 0) throw IOException(ptmx,"unlockpt",errno);

    bool dolink = true;
    struct stat linkstat;
    if (lstat(link.c_str(),&linkstat) < 0) {
        if (errno != ENOENT)
		throw IOException(link,"stat",errno);
    }
    else {
        if (S_ISLNK(linkstat.st_mode)) {
	    char linkdest[MAXPATHLEN];
	    int ld = readlink(link.c_str(),linkdest,MAXPATHLEN-1);
	    if (ld < 0)
		throw IOException(link,"readlink",errno);
	    linkdest[ld] = 0;
	    if (strcmp(slave,linkdest)) {
		cerr << "Deleting " << link << " (a symbolic link to " << linkdest << ")" << endl;
		if (unlink(link.c_str()) < 0)
		    throw IOException(link,"unlink",errno);
	    }
	    else dolink = false;
	}
	else
	    throw IOException(link,
	    	"exists and is not a symbolic link","");

    }
    if (dolink) {
	cerr << "Linking " << slave << " to " << link << endl;
        if (symlink(slave,link.c_str()) < 0)
	    throw IOException(link,"symlink",errno);
    }
    return fd;
}

