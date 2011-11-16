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
