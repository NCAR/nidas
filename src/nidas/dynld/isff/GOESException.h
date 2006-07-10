//
//              Copyright 2004 (C) by UCAR
//

#ifndef NIDAS_DYNLD_ISFF_GOESEXCEPTION_H
#define NIDAS_DYNLD_ISFF_GOESEXCEPTION_H

#include <nidas/util/IOException.h>

namespace nidas { namespace dynld { namespace isff {

class GOESException : public nidas::util::IOException {
public:
    GOESException(const std::string& device,const std::string& task,
  	const std::string& msg, int stat):
	nidas::util::IOException("GOESException",device,task,msg),
	status(stat) {}

    int getStatus() const { return status; }

private:
    int status;
};

} } }	// namespace nidas namespace dynld namespace isff

#endif
