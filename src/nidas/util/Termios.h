/*              Copyright (C) by UCAR
 *
 * Description:
 */

#ifndef NIDAS_UTIL_TERMIOS_H
#define NIDAS_UTIL_TERMIOS_H

/*
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
*/
#include <termios.h>
#include <sys/ioctl.h>

#include <nidas/util/IOException.h>

#include <string>

namespace nidas { namespace util {

class SerialOptions;

/**
 * A class providing get/set methods into a termios structure.
 */
class Termios {

public:

  Termios();

  Termios(const struct termios*);

  const struct termios* getTermios(int fd,const std::string& devname)
  	throw(IOException);

  void setTermios(int fd,const std::string& devname)
  	throw(IOException);

  void setTermios(const struct termios*);

  const struct termios* getTermios();

  bool setBaudRate(int val);
  int getBaudRate() const;

  enum parity { NONE, ODD, EVEN};

  void setParity(enum parity val);
  parity getParity() const;
  std::string getParityString() const;

  /**
   * Set number of data bits to 5,6,7 or 8.
   */
  void setDataBits(int val);
  int getDataBits() const;

  /**
   * Set number of stop bits, to 1 or 2.
   */
  void setStopBits(int val);
  int getStopBits() const;

  /**
   * If local, then ignore carrier detect modem control line.
   */
  void setLocal(bool val);
  bool getLocal() const;

  /**
   * HARDWARE flow control is CTSRTS. SOFTWARE is Xon/Xoff.
   */
  enum flowcontrol { NOFLOWCONTROL, HARDWARE, SOFTWARE };
  typedef enum flowcontrol flowcontrol;

  /**
   * Set flow control to NOFLOWCONTROL, HARDWARE or SOFTWARE.
   */
  void setFlowControl(flowcontrol val);
  flowcontrol getFlowControl() const;
  std::string getFlowControlString() const;

  /**
   * Sets a bunch of termios options for raw or non-raw(cooked) mode.
   */
  void setRaw(bool val);
  bool getRaw() const;

  void setRawLength(unsigned char val);
  unsigned char getRawLength() const;

  void setRawTimeout(unsigned char val);
  unsigned char getRawTimeout() const;

  void setOptions(const SerialOptions& opts);

  tcflag_t &iflag() { return tio.c_iflag; }
  tcflag_t &oflag() { return tio.c_oflag; }
  tcflag_t &cflag() { return tio.c_cflag; }
  tcflag_t &lflag() { return tio.c_lflag; }
  cc_t *cc() { return tio.c_cc; }

  static struct baudtable {
    unsigned int cbaud;
    int rate;
  } bauds[];

  void setDefaultTermios();

private:
  struct termios tio;
  unsigned char rawlen;
  unsigned char rawtimeout;
};

}}	// namespace nidas namespace util

#endif
