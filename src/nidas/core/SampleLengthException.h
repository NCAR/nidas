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

#ifndef NIDAS_CORE_SAMPLELENGTHEXCEPTION_H
#define NIDAS_CORE_SAMPLELENGTHEXCEPTION_H

#include <nidas/util/Exception.h>
#include <sstream>

namespace nidas { namespace core {

class SampleLengthException : public nidas::util::Exception {
public:
    SampleLengthException(const std::string& msg,size_t val, size_t maxVal):
        nidas::util::Exception("")
    {
        std::ostringstream os;
        os << "SampleLengthException: " << msg << ": value=" << val <<
            " exceeds maximum allowed value=" << maxVal;
        _what = os.str();
    }
    /**
     * Copy constructor.
     */
    SampleLengthException(const SampleLengthException& e) : Exception(e) {}

    virtual ~SampleLengthException() throw() {}

    virtual SampleLengthException* clone() const {
        return new SampleLengthException(*this);
    }

};

}}	// namespace nidas namespace core

#endif
