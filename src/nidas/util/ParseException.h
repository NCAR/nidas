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

#ifndef NIDAS_UTIL_PARSEEXCEPTION_H
#define NIDAS_UTIL_PARSEEXCEPTION_H

#include <string>
#include <sstream>
#include <nidas/util/Exception.h>

namespace nidas { namespace util {

  class ParseException : public Exception {

  public:
 
    /**
     * Create a ParseException, passing a name of what you're trying
     * to parse, a message, and the line number of the document.
     */
    ParseException(const std::string& what,
	const std::string& msg, int lineno):
      Exception("ParseException", what + ": " +
		msg + " at line " + linenoToString(lineno)) {}

    /**
     * Create a ParseException, passing a name of what you're trying
     * to parse, and a message.
     */
    ParseException(const std::string& what,
	const std::string& msg):
      Exception("ParseException", what + ": " + msg) {}

    /**
     * Create an ParseException, passing a message.
     */
    ParseException(const std::string& message):
      Exception(message) {}

    /**
     * clone myself (a "virtual" constructor).
     */
    Exception* clone() const {
      return new ParseException(*this);
    }

  private:
    static std::string linenoToString(int lineno) {
      std::ostringstream os;
      os << lineno;
      return os.str();
    }
  };

}}	// namespace nidas namespace util

#endif
