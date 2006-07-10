#ifndef NIDAS_UTIL_SERIALOPTIONS_H
#define NIDAS_UTIL_SERIALOPTIONS_H

// #include <string>

#include <sys/types.h>
#include <regex.h>

#include <nidas/util/SerialPort.h>
#include <nidas/util/ParseException.h>

namespace nidas { namespace util {

class SerialOptions {

public:

  SerialOptions() throw(ParseException);
  ~SerialOptions();

  void parse(const std::string& input) throw(ParseException);

  int getBaudRate() const { return baud; }
  SerialPort::parity getParity() const { return parity; }
  int getDataBits() const { return dataBits; }
  int getStopBits() const { return stopBits; }
  bool getLocal() const { return local; }
  SerialPort::flowcontrol getFlowControl() const { return flowControl; }
  bool getRaw() const { return raw; }

  tcflag_t getNewlineIflag() const { return iflag; }
  tcflag_t getNewlineOflag() const { return oflag; }

  std::string toString() const;

  static const char* usage();

private:
  const char* regexpression;
  regex_t compRegex;
  int compileResult;
  int nmatch;

  int baud;
  SerialPort::parity parity;
  int dataBits;
  int stopBits;
  bool local;
  SerialPort::flowcontrol flowControl;
  bool raw;

  tcflag_t iflag;
  tcflag_t oflag;
};

}}	// namespace nidas namespace util

#endif
