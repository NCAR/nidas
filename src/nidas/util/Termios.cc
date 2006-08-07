//              Copyright (C) by UCAR
//
// Description:
//

#ifdef GPP_2_95_2
#include <strstream>
#else
#include <sstream>
#endif

#include <nidas/util/Termios.h>
#include <nidas/util/SerialOptions.h>
#include <sys/ioctl.h>
#include <cerrno>

using namespace std;
using namespace nidas::util;

/* static */
Termios::baudtable Termios::bauds[] = {
    { B0,       0},
    { B50,      50},
    { B75,      75},
    { B110,     110},
    { B134,     134},
    { B150,     150},
    { B200,     200},
    { B300,     300},
    { B600,     600},
    { B1200,    1200},
    { B1800,    1800},
    { B2400,    2400},
    { B4800,    4800},
    { B9600,    9600},
    { B19200,   19200},
    { B38400,   38400},
    { B57600,   57600},
    { B115200,  115200},
    { B230400,  230400},
#ifdef B76800
    { B76800,   76800},
#endif
#ifdef B153600
    { B153600,  153600},
#endif
#ifdef B307200
    { B307200,  307200},
#endif
    { B460800,  460800},
    { B0,  -1}
};

Termios::Termios()
{
  setDefaultTermios();
}

Termios::Termios(const struct termios* termios_p)
{
  tio = *termios_p;
}

const struct termios*
Termios::getTermios(
	int fd,const string& name) throw(IOException) {
  if (::tcgetattr(fd, &tio) < 0)
    throw IOException(name ,"tcgetattr",errno);
  if (!(tio.c_lflag & ICANON)) {
	rawlen = tio.c_cc[VMIN];
	rawtimeout = tio.c_cc[VTIME];
  }
  return &tio;
}

void
Termios::setTermios(int fd, const string& name) throw(IOException)
{
  if (::tcsetattr(fd, TCSANOW, &tio) < 0)
    throw IOException(name,"tcsetattr",errno);
}

const struct termios*
Termios::getTermios()
{
  return &tio;
}

void
Termios::setTermios(const struct termios* termios_p)
{
  tio = *termios_p;
}


void
Termios::setDefaultTermios()
{
  memset(&tio,0,sizeof(tio));
  tio.c_iflag = IGNBRK | ICRNL;
  tio.c_cflag = CS8 | CLOCAL | CREAD | B9600;
  tio.c_oflag = OPOST | ONLCR;
  tio.c_lflag = ICANON | ISIG | ECHOE | ECHOCTL | IEXTEN;
  tio.c_cc[VINTR] = '\003';
  tio.c_cc[VQUIT] = '\034';
  tio.c_cc[VERASE] = '\177';
  tio.c_cc[VKILL] = '\025';
  tio.c_cc[VEOF] = '\004';
  tio.c_cc[VEOL] = 0;
  cfsetispeed(&tio,B9600);
  cfsetospeed(&tio,B9600);
  rawlen = 0;
  rawtimeout = 0;
  // std::cerr << "cbaud=" << std::oct << (tio.c_cflag & (CBAUD | CBAUDEX)) << std::dec << std::endl;
}


void
Termios::setOptions(const SerialOptions& sopts) {

  setBaudRate(sopts.getBaudRate());
  setParity(sopts.getParity());
  setDataBits(sopts.getDataBits());
  setStopBits(sopts.getStopBits());
  setLocal(sopts.getLocal());
  setFlowControl(sopts.getFlowControl());
  setRaw(sopts.getRaw());

  // turn CRNL->NL and NL->CRNL conversions back on
  if (!sopts.getRaw()) {
    iflag() |= sopts.getNewlineIflag();
    oflag() |= OPOST | sopts.getNewlineOflag();
  }
}


bool
Termios::setBaudRate(int val)
{

  int i;
  speed_t cbaud = B9600;
  for (i = 0; bauds[i].rate >= 0; i++)
    if (bauds[i].rate == val) {
      cbaud = bauds[i].cbaud;
      break;
    }
  if (bauds[i].rate < 0) return false;

  tio.c_cflag &= ~(CBAUD | CBAUDEX);
  tio.c_cflag |= cbaud;

  // std::cerr << "cbaud=" << std::oct << (tio.c_cflag & (CBAUD | CBAUDEX)) << std::dec << std::endl;
  cfsetispeed(&tio,cbaud);
  cfsetospeed(&tio,cbaud);
  return true;
}

int Termios::getBaudRate() const
{ 
  speed_t cbaud = cfgetispeed(&tio);
  cbaud = tio.c_cflag & (CBAUD | CBAUDEX);
  int i;
  for (i = 0; bauds[i].rate >= 0; i++)
    if (bauds[i].cbaud == cbaud) return bauds[i].rate;
  return 0;
}

void Termios::setParity(enum parity val) {
  switch (val) {
  case NONE:
    tio.c_cflag &= ~PARENB;
    // disable parity checking
    tio.c_iflag &= ~(INPCK);
    break;
  case EVEN:
    tio.c_cflag |= PARENB;
    tio.c_cflag &= ~PARODD;
    tio.c_iflag |= INPCK;
    // don't ignore parity errors, but don't mark them either
    tio.c_iflag &= ~(IGNPAR | PARMRK);
    break;
  case ODD:
    tio.c_cflag |= PARENB;
    tio.c_cflag |= PARODD;
    tio.c_iflag |= INPCK;
    tio.c_iflag &= ~(IGNPAR | PARMRK);
    break;
  }
}

Termios::parity
Termios::getParity() const
{
  if (!(tio.c_cflag & PARENB)) return NONE;
  if (tio.c_cflag & PARODD) return ODD;
  return EVEN;
}

void
Termios::setDataBits(int val)
{
  switch (val) {
  case 5: tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS5; break;
  case 6: tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS6; break;
  case 7:
    tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS7;
    tio.c_iflag |= ISTRIP;
    break;
  default:
  case 8:
    tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS8;
    tio.c_iflag &= ~ISTRIP;
    break;
  }
}

int
Termios::getDataBits() const
{
  int csize = tio.c_cflag & CSIZE;
  switch(csize) {
    case CS5: return 5;
    case CS6: return 6;
    case CS7: return 7;
    default: return 8;
  }
}

void
Termios::setStopBits(int val)
{
  switch (val) {
  default:
  case 1: tio.c_cflag &= ~CSTOPB; break;
  case 2: tio.c_cflag |= CSTOPB; break;
  }
}

int Termios::getStopBits() const
{
  if (tio.c_cflag & CSTOPB) return 2;
  return 1;
}

void
Termios::setLocal(bool val)
{
  if (val) {
    tio.c_cflag |= CLOCAL;
    tio.c_cflag &= ~HUPCL;
  }
  else {
    tio.c_cflag &= ~CLOCAL;
    tio.c_cflag |= HUPCL;
  }
}

bool Termios::getLocal() const
{
  return tio.c_cflag & CLOCAL;
}

void
Termios::setFlowControl(flowcontrol val)
{
  switch (val) {
  case NOFLOWCONTROL:
    tio.c_cflag &= ~CRTSCTS;
    tio.c_iflag &= ~IXON;
    tio.c_iflag &= ~IXOFF;
    tio.c_iflag &= ~IXANY;
    break;
  case HARDWARE:
    tio.c_cflag |= CRTSCTS;
    tio.c_iflag &= ~IXON;
    tio.c_iflag &= ~IXOFF;
    tio.c_iflag &= ~IXANY;
    break;
  case SOFTWARE:
    tio.c_cflag &= ~CRTSCTS;
    tio.c_iflag |= IXON;
    tio.c_iflag |= IXOFF;
    tio.c_iflag |= IXANY;
    break;
  }
}

Termios::flowcontrol
Termios::getFlowControl() const
{
  if (tio.c_cflag & CRTSCTS) return HARDWARE;
  if (tio.c_iflag & IXON || tio.c_iflag & IXOFF || tio.c_iflag & IXANY)
  	return SOFTWARE;
  return NOFLOWCONTROL;
}

void
Termios::setRaw(bool val)
{
  if (val) {
    tio.c_iflag |= IGNBRK;
    /*
     * Watch out for meaning of IGNCR:  If the bit is set then
     * CR are tossed! It doesn't mean turn off translations!
     */
    tio.c_iflag &= ~(IGNCR | INLCR | ICRNL | IUCLC );
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ICANON | ISIG | ECHO | ECHOE | IEXTEN | XCASE);
    tio.c_cc[VMIN] = rawlen;
    tio.c_cc[VTIME] = rawtimeout;
  }
  else {
    tio.c_iflag |= IGNBRK | ICRNL;
    tio.c_iflag &= ~(IGNCR | INLCR | IUCLC );
    tio.c_oflag |= OPOST | ONLCR;
    tio.c_lflag |= (ICANON | ISIG | ECHOE | ECHOCTL | IEXTEN);
    tio.c_lflag &= ~ECHO;
    tio.c_cc[VEOL] = 0x0;
    tio.c_cc[VEOL2] = 0x0;
    tio.c_cc[VEOF] = 0x04;
  }
}

bool
Termios::getRaw() const
{
  if (tio.c_oflag & OPOST || tio.c_lflag & ICANON) return false;
  return true;
}

void
Termios::setRawLength(unsigned char val)
{
  rawlen = val;
  tio.c_cc[VMIN] = rawlen;
}

void
Termios::setRawTimeout(unsigned char val)
{
  rawtimeout = val;
  tio.c_cc[VTIME] = rawtimeout;
}

unsigned char
Termios::getRawLength() const
{
  return tio.c_cc[VMIN];
}

unsigned char
Termios::getRawTimeout() const
{
  return tio.c_cc[VTIME];
}

std::string Termios::getParityString() const {
  switch(getParity()) {
    case NONE: return "none"; break;
    case ODD: return "odd";
    case EVEN: return "even";
  }
  return "unknown";
}

std::string Termios::getFlowControlString() const {
  switch(getFlowControl()) {
    case NOFLOWCONTROL: return "none"; break;
    case HARDWARE: return "hardware";
    case SOFTWARE: return "software";
  }
  return "unknown";
}

