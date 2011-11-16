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
