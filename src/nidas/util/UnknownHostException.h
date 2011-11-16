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
