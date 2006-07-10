//
//              Copyright 2004 (C) by UCAR
//

#ifndef NIDAS_UTIL_UNKNOWNHOSTEXCEPTION
#define NIDAS_UTIL_UNKNOWNHOSTEXCEPTION

#include <string>
#include <nidas/util/Exception.h>

namespace nidas { namespace util {

class UnknownHostException : public Exception {
public:
  UnknownHostException(const std::string& host) :
    Exception("UnknownHostException",host) {}
};

}}	// namespace nidas namespace util

#endif
