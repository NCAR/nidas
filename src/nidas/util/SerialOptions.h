// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ********************************************************************
 */

#ifndef NIDAS_UTIL_SERIALOPTIONS_H
#define NIDAS_UTIL_SERIALOPTIONS_H

// #include <string>

#include <sys/types.h>
#include <regex.h>

#include <nidas/util/Termios.h>
#include <nidas/util/ParseException.h>

namespace nidas { namespace util {

/**
 * Class providing a method to parse a string into a Termios.
 * An example of a parseable string is:
 *      9600n81lncnc : 9600 baud, no parity, local, no flow control\n\
 */
class SerialOptions {

public:

    SerialOptions() throw(ParseException);

    ~SerialOptions();

    /**
     * Parse a string into a Termios object.
     * The format of the string is as follows, with no spaces between the values:
     *  baud parity data stop local_modem flow_control raw_cooked newline_opts
     *
     *  baud: 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, etc bits/sec
     *  parity:  n=none, o=odd, e=even
     *  data: number of data bits, 8 or 7
     *  stop: number of stop bits, 1 or 2
     *  local_modem: l=local (ignore carrier detect), m=modem (monitor CD)
     *  flow_control: n=none, h=hardware (CTS/RTS), s=software (XON/XOFF)
     *  raw_cooked:
     *      r=raw (no change to input or output characters, binary data)
     *      c=cooked (scan input and output for special characters)
     *  newline_opts: input option followed by output option, and is
     *      only necessary if "cooked" option is enabled.
     *      input option:
     *          n=convert input carriage-return (CR) to new-line (NL)
     *          c=convert input NL to CR
     *          d=discard input CRs
     *          x=no change to CRs
     *      output option:
     *          n=convert output CR to NL
     *          c=convert output NL to CR
     *          x=no change to CR
     *  Example:
     *  9600n81lncnc : 9600 baud, no parity, local, no flow control
     *      cooked, convert input CR->NL, output NL->CR (unix terminal)
     */
    void parse(const std::string& input) throw(ParseException);

    const Termios& getTermios() const { return _termios; }

    std::string toString() const;

    static const char* usage();

private:
    static const char* _regexpression;

    regex_t _compRegex;

    int _compileResult;

    int _nmatch;

    Termios _termios;
};

}}	// namespace nidas namespace util

#endif
