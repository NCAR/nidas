//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifndef NIDAS_UTIL_EXCEPTION_H
#define NIDAS_UTIL_EXCEPTION_H

#include <string>

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
    Exception(const std::string& n, const std::string& m) : _what(n + ": " + m) {}

    Exception(const std::string& m, int ierr):
    	_what(m + ": " + errnoToString(ierr)) {}

    Exception(const std::string& m) : _what(m) {}

    /**
     * Copy constructor.
     */
    Exception(const Exception& e) : _what(e._what) {}

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
