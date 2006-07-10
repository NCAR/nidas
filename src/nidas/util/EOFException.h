//
//              Copyright 2004 (C) by UCAR
//

#ifndef NIDAS_UTIL_EOFEXCEPTION_H
#define NIDAS_UTIL_EOFEXCEPTION_H

#include <nidas/util/IOException.h>

namespace nidas { namespace util {

class EOFException : public IOException {
public:
  EOFException(const std::string& device,const std::string& task):
    IOException("EOFException",device,task,"EOF") {}
};

} }	// namespace nidas namespace util

#endif
