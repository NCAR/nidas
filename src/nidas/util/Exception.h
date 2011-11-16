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

  public:

    /**
     * Constructor for an exception.
     * Typical use:
     *	<tt>Exception e("RealBadSituation","help!");</tt>
     */
    Exception(const std::string& n, const std::string& m):
        std::exception(), _what(n + ": " + m) {}

    Exception(const std::string& m, int ierr):
    	std::exception(),_what(m + ": " + errnoToString(ierr)) {}

    Exception(const std::string& m):
        std::exception(),_what(m) {}

    /**
     * Copy constructor.
     */
    Exception(const Exception& e):
        std::exception(e),_what(e._what) {}

    virtual ~Exception() throw() {}

    virtual Exception* clone() const {
      return new Exception(*this);
    }

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
