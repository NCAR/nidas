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
#include "SerialOptions.h"
#include <cstdlib>  // atoi()

using namespace nidas::util;
using namespace std;

/*
 * newline options (only used in cooked mode):
 *   two characters, input option followed by output option
 * input: n = cr->nl(ICRNL), c = nl->cr(INLCR), x = cr->nothing(IGNCR)
 * output: c = nl->crnl(ONLCR), n = cr->nl(OCRNL)
 *  example:
 *    nc: cr->nl on input, nl->crnl on output (std unix terminal)
 */
const char* SerialOptions::_regexpression =
    /*     baud      par    data   stop  local/ flow   raw/    newline*/
    /*                                   modem  cntl   cook */
        "^([0-9]+)([neo])([78])([12])([lm])([nhs])([rc])([ncdx][ncx])?$";

SerialOptions::SerialOptions() throw(ParseException) :
    _compRegex(),_compileResult(),_nmatch(0), _termios()
{
    int cflags = REG_EXTENDED;	// REG_EXTENDED, REG_ICASE, REG_NOSUB, REG_NEWLINE
    _compileResult = regcomp(&_compRegex,_regexpression,cflags);
    _nmatch = 9;		// one plus number of paretheses expressions above
}

SerialOptions::~SerialOptions()
{
    regfree(&_compRegex);
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
    if (_compileResult != 0) {
        size_t i = regerror(_compileResult,&_compRegex,0,0);
        char errstr[i];
        regerror(_compileResult,&_compRegex,errstr,i);
        throw ParseException(errstr);
    }

    regmatch_t matches[_nmatch];
    int eflags = 0;
    if (regexec(&_compRegex,input.c_str(),_nmatch,matches,eflags) != 0)
        throw ParseException(string("input \"") + input +
                "\" does not match regular expression \"" + _regexpression + "\"");

    string val;
    int imtch = 1;

    if (matches[imtch].rm_so == -1) throw ParseException(
            string("no baud rate value \"") + val + "\" in \"" + input + "\"");
    val = input.substr(matches[imtch].rm_so,
            matches[imtch].rm_eo-matches[imtch].rm_so);
    int baud = atoi(val.c_str());
    if (baud == 0) throw ParseException(
            string("unparseable baud rate value \"") + val + "\" in \"" + input + "\"");
    imtch++;
    if (!_termios.setBaudRate(baud)) {
        throw ParseException(
            string("unsupported baud rate value \"") + val + "\" in \"" + input + "\"");
    }

    if (matches[imtch].rm_so == -1) throw ParseException(
            string("parity value not found in \"") + input + "\"");
    val = input.substr(matches[imtch].rm_so,
            matches[imtch].rm_eo-matches[imtch].rm_so);
    Parity parity;
    switch(val.at(0)) {
    case 'n': parity = Parity::NONE; break;
    case 'e': parity = Parity::EVEN; break;
    case 'o': parity = Parity::ODD; break;
    default:
              throw ParseException(
                      string("invalid parity value \'") + val + "\' in \"" + input + "\"");
    }
    imtch++;
    _termios.setParity(parity);

    if (matches[imtch].rm_so == -1) throw ParseException(
            string("data bits value not found in \"") + input + "\"");
    val = input.substr(matches[imtch].rm_so,
            matches[imtch].rm_eo-matches[imtch].rm_so);
    int bits = atoi(val.c_str());
    if (bits == 0) throw ParseException(
            string("unparseable number of data bits \"") + val + "\" in \"" + input + "\"");
    imtch++;
    _termios.setDataBits(bits);

    if (matches[imtch].rm_so == -1) throw ParseException(
            string("stop bits value not found in \"") + input + "\"");
    val = input.substr(matches[imtch].rm_so,
            matches[imtch].rm_eo-matches[imtch].rm_so);
    bits = atoi(val.c_str());
    if (bits == 0) throw ParseException(
            string("unparseable number of stop bits \"") + val + "\" in \"" + input + "\"");
    imtch++;
    _termios.setStopBits(bits);

    if (matches[imtch].rm_so == -1) throw ParseException(
            string("local/modem field not found in \"") + input + "\"");
    val = input.substr(matches[imtch].rm_so,
            matches[imtch].rm_eo-matches[imtch].rm_so);
    bool local;
    switch(val.at(0)) {
    case 'l': local = true; break;
    case 'm': local = false; break;
    default:
              throw ParseException(
                      string("invalid local/modem value \'") + val + "\' in \"" + input + "\"");
    }
    imtch++;
    _termios.setLocal(local);

    if (matches[imtch].rm_so == -1) throw ParseException(
            string("flow control field not found in \"") + input + "\"");
    val = input.substr(matches[imtch].rm_so,
            matches[imtch].rm_eo-matches[imtch].rm_so);
    Termios::flowcontrol flowControl;
    switch(val.at(0)) {
    case 'n': flowControl = Termios::NOFLOWCONTROL; break;
    case 'h': flowControl = Termios::HARDWARE; break;
    case 's': flowControl = Termios::SOFTWARE; break;
    default:
              throw ParseException(
                      string("invalid flow control value \'") + val + "\' in \"" + input + "\"");
    }
    imtch++;
    _termios.setFlowControl(flowControl);

    if (matches[imtch].rm_so == -1) throw ParseException(
            string("raw/cooked field not found in \"") + input + "\"");
    val = input.substr(matches[imtch].rm_so,
            matches[imtch].rm_eo-matches[imtch].rm_so);
    bool raw;
    switch(val.at(0)) {
    case 'r': raw = true; break;
    case 'c': raw = false; break;
    default:
              throw ParseException(
                      string("invalid raw/cooked value \'") + val + "\' in \"" + input + "\"");
    }
    imtch++;
    _termios.setRaw(raw);

    // input: n = cr->nl,ICRNL, c = nl->cr,INLCR, d = cr->nothing,IGNCR, x = no change
    // output: c = nl->crnl,ONLCR, n = cr->nl,OCRNL, x = no change
    if (matches[imtch].rm_so != -1 &&
            matches[imtch].rm_eo - matches[imtch].rm_so > 0) {
        val = input.substr(matches[imtch].rm_so,
                matches[imtch].rm_eo-matches[imtch].rm_so);
        // disable all these iflags, then enable individual flags as requested.
        _termios.iflag() &= ~(ICRNL | INLCR | IGNCR);
        switch(val.at(0)) {
        case 'n': _termios.iflag() |= ICRNL; break;
        case 'c': _termios.iflag() |= INLCR; break;
        case 'd': _termios.iflag() |= IGNCR; break;
        case 'x': break;
        default:
                  throw ParseException(
                          string("invalid input carriage-return/newline value \'") + val + "\' in \"" + input + "\"");
        }
        if (matches[imtch].rm_eo - matches[imtch].rm_so > 1) {
            // disable all these iflags, then enable individual flags as requested.
            _termios.oflag() &= ~(OCRNL | ONLCR);
            switch(val.at(1)) {
            case 'n': _termios.oflag() |= OCRNL; break;
            case 'c': _termios.oflag() |= ONLCR; break;
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

    ost << "baud=" << _termios.getBaudRate() << endl;
    ost << "parity=" << _termios.getParity() << endl;
    ost << "databits=" << _termios.getDataBits() << endl;
    ost << "stopbits=" << _termios.getStopBits() << endl;
    ost << "local=" << _termios.getLocal() << endl;
    ost << "flowcontrol=" <<
        ((_termios.getFlowControl() == Termios::NOFLOWCONTROL) ? "none" :
         ((_termios.getFlowControl() == Termios::HARDWARE) ? "hard " : "soft")) << endl;
    ost << "raw=" << _termios.getRaw() << endl;
    ost << "iflag=" << hex << _termios.getIflag() << endl;
    ost << "oflag=" << hex << _termios.getOflag() << endl;

    return ost.str();
}
