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

#ifndef NIDAS_UTIL_EXCEPTION_H
#define NIDAS_UTIL_EXCEPTION_H

#include <string>
#include <errno.h>

namespace nidas { namespace util {

class Exception : public std::exception {

protected:
    std::string _what;
    int _errno;

    Exception(const std::string& type, const std::string& n, const std::string& m):
        std::exception(), _what(type + ": " + n + ": " + m),_errno(0) {}

    Exception(const std::string& type, const std::string& n, int ierr):
        std::exception(), _what(type + ": " + n + ": " + errnoToString(ierr)),_errno(ierr) {}

public:

    /**
     * Constructor for an exception.
     * Typical use:
     *	<tt>Exception e("RealBadSituation","help!");</tt>
     */
    Exception(const std::string& n, const std::string& m):
        std::exception(), _what("Exception: " + n + ": " + m),_errno(0) {}

    /**
     * Constructor for an exception.
     * Typical use:
     *	<tt>Exception e("fork",errno);</tt>
     */
    Exception(const std::string& m, int ierr):
        std::exception(),_what("Exception: " + m + ": " + errnoToString(ierr)),_errno(ierr) {}

    Exception(const std::string& m):
        std::exception(),_what(m),_errno(0) {}

    /**
     * Copy constructor.
     */
    Exception(const Exception& e):
        std::exception(e),_what(e._what),_errno(e._errno) {}

    virtual ~Exception() throw() {}

    virtual Exception* clone() const {
        return new Exception(*this);
    }

    virtual int getErrno() const { return _errno; }

    /**
     * Return string description of an errno (from errno.h).
     * A convienence method that uses strerror_r(int) from <string.h>.
     */
    static std::string errnoToString(int err);

    virtual std::string toString() const throw() { return _what; }

    virtual const char* what() const throw() { return _what.c_str(); }
};

}}	// namespace nidas::util

#endif
