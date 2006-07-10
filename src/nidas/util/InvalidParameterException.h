//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

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
