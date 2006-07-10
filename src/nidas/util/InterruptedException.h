//
//              Copyright 2004 (C) by UCAR
//
//
// Description:
//

#ifndef NIDAS_UTIL_INTERRUPTEDEXCEPTION_H
#define NIDAS_UTIL_INTERRUPTEDEXCEPTION_H

#include <string>
#include <nidas/util/Exception.h>

namespace nidas { namespace util {


class InterruptedException : public Exception {
public:
  InterruptedException(const std::string& what, const std::string& task):
    Exception("InterruptedException: ",what + ": " + task) {}
};

}}	// namespace nidas namespace util

#endif
