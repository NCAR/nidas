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

#ifndef NIDAS_UTIL_INVALIDPARAMETEREXCEPTION_H
#define NIDAS_UTIL_INVALIDPARAMETEREXCEPTION_H

#include <string>
#include <nidas/util/Exception.h>

namespace nidas { namespace util {

  class InvalidParameterException : public Exception {

  public:
 
    /**
     * Create an InvalidParameterException, passing a name of the
     * software or hardware module, the name of the parameter, and
     * a message.
     */
    InvalidParameterException(const std::string& module,
	const std::string& name, const std::string& msg):
      Exception("InvalidParameterException", module + ": " +
		name + ": " + msg)
    {}

    /**
     * Create an InvalidParameterException, passing a message.
     */
    InvalidParameterException(const std::string& message):
      Exception(message) {}

    /**
     * clone myself (a "virtual" constructor).
     */
    Exception* clone() const {
      return new InvalidParameterException(*this);
    }
  };

}}	// namespace nidas namespace util

#endif
