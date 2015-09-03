// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_SAMPLEPARSEEXCEPTION_H
#define NIDAS_CORE_SAMPLEPARSEEXCEPTION_H

#include <string>
#include <nidas/util/Exception.h>

namespace nidas { namespace core {

class SampleParseException : public nidas::util::Exception
{

  public:
 
    /**
     * Create a ParseException, passing a name of what you're trying
     * to parse, a message, and the line number of the document.
     */
    SampleParseException(const std::string& what,
	const std::string& msg):
      nidas::util::Exception("SampleParseException", what + ": " + msg) {}

    /**
     * Create an ParseException, passing a message.
     */
    SampleParseException(const std::string& message):
      nidas::util::Exception(message) {}

    /**
     * clone myself (a "virtual" constructor).
     */
    SampleParseException* clone() const {
	return new SampleParseException(*this);
    }
};

}}	// namespace nidas namespace core

#endif
