//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifndef NIDAS_UTIL_IOTIMEOUTEXCEPTION_H
#define NIDAS_UTIL_IOTIMEOUTEXCEPTION_H

#include <string>
#include <nidas/util/IOException.h>

namespace nidas { namespace util {

class IOTimeoutException : public IOException {
public:
    IOTimeoutException(const std::string& device,const std::string& task):
	IOException("IOTimeoutException",device,task,"timeout") {}
};

}}	// namespace nidas namespace util

#endif
