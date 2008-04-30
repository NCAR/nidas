#include <nidas/util/SerialOptions.h>
#include <stdlib.h>  // atoi()

using namespace nidas::util;
using namespace std;

SerialOptions::SerialOptions() throw(ParseException) :
	baud(0),parity(SerialPort::NONE),dataBits(8),
	stopBits(1),local(true),flowControl(SerialPort::NOFLOWCONTROL),
	raw(true),iflag(0),oflag(0)
{
  /*
   * newline options (only used in cooked mode):
   *   two characters, input option followed by output option
   * input: n = cr->nl(ICRNL), c = nl->cr(INLCR), x = cr->nothing(IGNCR)
   * output: c = nl->crnl(ONLCR), n = cr->nl(OCRNL)
   *  example:
   *    nc: cr->nl on input, nl->crnl on output (std unix terminal)
   */

  /*     baud      par    data   stop  local/ flow   raw/    newline*/
  /*                                   modem  cntl   cook */
  regexpression =
    "^([0-9]+)([neo])([78])([12])([lm])([nhs])([rc])([ncdx][ncx])?$";

  int cflags = REG_EXTENDED;	// REG_EXTENDED, REG_ICASE, REG_NOSUB, REG_NEWLINE
  compileResult = regcomp(&compRegex,regexpression,cflags);
  nmatch = 9;		// one plus number of paretheses expressions above
}

SerialOptions::~SerialOptions()
{
  regfree(&compRegex);
}

/* static */
const char* SerialOptions::usage() {
  return "format of SerialOptions string (no spaces between values):\n\
baud parity data stop local_modem flow_control raw_cooked newline_opts\n\
  baud: 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, etc bits/sec\n\
  parity:  n=none, o=odd, e=even\n\
  data: number of data bits, 8 or 7\n\
  stop: number of stop bits, 1 or 2\n\
  local_modem: l=local (ignore carrier detect), m=modem (monitor CD)\n\
  flow_control: n=none, h=hardware (CTS/RTS), s=software (XON/XOFF)\n\
  raw_cooked: r=raw (don't change input or output, binary data),\n\
              c=cooked (scan input and output for special characters)\n\
  newline_opts: input option followed by output option\n\
    only necessary if \"cooked\" option is enabled\n\
    input option:  n=convert input carriage-return (CR) to new-line (NL)\n\
		   c=convert input NL to CR\n\
		   d=discard input CRs\n\
		   x=no change to CRs\n\
    output option: n=convert output CR to NL\n\
		   c=convert output NL to CR\n\
		   x=no change to CR\n\
example:\n\
  9600n81lncnc : 9600 baud, no parity, local, no flow control\n\
    cooked, convert input CR->NL, output NL->CR (unix terminal)\n";
}

void SerialOptions::parse(const string& input) throw(ParseException)
{
  if (compileResult != 0) {
    size_t i = regerror(compileResult,&compRegex,0,0);
    char errstr[i];
    regerror(compileResult,&compRegex,errstr,i);
    throw ParseException(errstr);
  }

  regmatch_t matches[nmatch];
  int eflags = 0;
  if (regexec(&compRegex,input.c_str(),nmatch,matches,eflags) != 0)
    throw ParseException(string("input \"") + input +
    	"\" does not match regular expression \"" + regexpression + "\"");

  string val;
  int imtch = 1;

  if (matches[imtch].rm_so == -1) throw ParseException(
      string("no baud rate value \"") + val + "\" in \"" + input + "\"");
  val = input.substr(matches[imtch].rm_so,
		matches[imtch].rm_eo-matches[imtch].rm_so);
  baud = 0;
  baud = atoi(val.c_str());
  if (baud == 0) throw ParseException(
      string("unparseable baud rate value \"") + val + "\" in \"" + input + "\"");
  imtch++;

  if (matches[imtch].rm_so == -1) throw ParseException(
    	string("parity value not found in \"") + input + "\"");
  val = input.substr(matches[imtch].rm_so,
		matches[imtch].rm_eo-matches[imtch].rm_so);
  switch(val.at(0)) {
  case 'n': parity = SerialPort::NONE; break;
  case 'e': parity = SerialPort::EVEN; break;
  case 'o': parity = SerialPort::ODD; break;
  default:
    throw ParseException(
      string("invalid parity value \'") + val + "\' in \"" + input + "\"");
  }
  imtch++;

  if (matches[imtch].rm_so == -1) throw ParseException(
    	string("data bits value not found in \"") + input + "\"");
  val = input.substr(matches[imtch].rm_so,
		matches[imtch].rm_eo-matches[imtch].rm_so);
  dataBits = 0;
  dataBits = atoi(val.c_str());
  if (dataBits == 0) throw ParseException(
      string("unparseable number of data bits \"") + val + "\" in \"" + input + "\"");
  imtch++;

  if (matches[imtch].rm_so == -1) throw ParseException(
    	string("stop bits value not found in \"") + input + "\"");
  val = input.substr(matches[imtch].rm_so,
		matches[imtch].rm_eo-matches[imtch].rm_so);
  stopBits = 0;
  stopBits = atoi(val.c_str());
  if (stopBits == 0) throw ParseException(
      string("unparseable number of stop bits \"") + val + "\" in \"" + input + "\"");
  imtch++;

  if (matches[imtch].rm_so == -1) throw ParseException(
    	string("local/modem field not found in \"") + input + "\"");
  val = input.substr(matches[imtch].rm_so,
		matches[imtch].rm_eo-matches[imtch].rm_so);
  switch(val.at(0)) {
  case 'l': local = true; break;
  case 'm': local = false; break;
  default:
    throw ParseException(
      string("invalid local/modem value \'") + val + "\' in \"" + input + "\"");
  }
  imtch++;

  if (matches[imtch].rm_so == -1) throw ParseException(
    	string("flow control field not found in \"") + input + "\"");
  val = input.substr(matches[imtch].rm_so,
		matches[imtch].rm_eo-matches[imtch].rm_so);
  switch(val.at(0)) {
  case 'n': flowControl = SerialPort::NOFLOWCONTROL; break;
  case 'h': flowControl = SerialPort::HARDWARE; break;
  case 's': flowControl = SerialPort::SOFTWARE; break;
  default:
    throw ParseException(
      string("invalid flow control value \'") + val + "\' in \"" + input + "\"");
  }
  imtch++;

  if (matches[imtch].rm_so == -1) throw ParseException(
    	string("raw/cooked field not found in \"") + input + "\"");
  val = input.substr(matches[imtch].rm_so,
		matches[imtch].rm_eo-matches[imtch].rm_so);
  switch(val.at(0)) {
  case 'r': raw = true; break;
  case 'c': raw = false; break;
  default:
    throw ParseException(
      string("invalid raw/cooked value \'") + val + "\' in \"" + input + "\"");
  }
  imtch++;

  // input: n = cr->nl,ICRNL, c = nl->cr,INLCR, d = cr->nothing,IGNCR, x = no change
  // output: c = nl->crnl,ONLCR, n = cr->nl,OCRNL, x = no change
  iflag = 0;
  oflag = 0;
  if (matches[imtch].rm_so != -1 &&
  	matches[imtch].rm_eo - matches[imtch].rm_so > 0) {
    val = input.substr(matches[imtch].rm_so,
		  matches[imtch].rm_eo-matches[imtch].rm_so);
    switch(val.at(0)) {
    case 'n': iflag |= ICRNL; break;
    case 'c': iflag |= INLCR; break;
    case 'd': iflag |= IGNCR; break;
    case 'x': break;
    default:
      throw ParseException(
	string("invalid input carriage-return/newline value \'") + val + "\' in \"" + input + "\"");
    }
    if (matches[imtch].rm_eo - matches[imtch].rm_so > 1) {
      switch(val.at(1)) {
      case 'n': oflag |= OCRNL; break;
      case 'c': oflag |= ONLCR; break;
      case 'x': break;
      default:
	throw ParseException(
	  string("invalid output carriage-return/newline value \'") + val + "\' in \"" + input + "\"");
      }
    }
  }
  imtch++;

}

string SerialOptions::toString() const {
  ostringstream ost;

  ost << "baud=" << getBaudRate() << endl;
  ost << "parity=" << (getParity() == SerialPort::EVEN ? "even" :
  	getParity() == SerialPort::ODD ? "odd " : "none") << endl;
  ost << "databits=" << getDataBits() << endl;
  ost << "stopbits=" << getStopBits() << endl;
  ost << "local=" << getLocal() << endl;
  ost << "flowcontrol=" <<
    ((getFlowControl() == SerialPort::NOFLOWCONTROL) ? "none" :
    ((getFlowControl() == SerialPort::HARDWARE) ? "hard " : "soft")) << endl;
  ost << "raw=" << getRaw() << endl;
  ost << "iflag=" << hex << getNewlineIflag() << endl;
  ost << "oflag=" << hex << getNewlineOflag() << endl;

  return ost.str();
}
