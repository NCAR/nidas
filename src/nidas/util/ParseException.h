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
