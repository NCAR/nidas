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
