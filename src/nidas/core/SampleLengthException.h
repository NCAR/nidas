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

#ifndef NIDAS_CORE_SAMPLELENGTHEXCEPTION_H
#define NIDAS_CORE_SAMPLELENGTHEXCEPTION_H

#include <nidas/util/Exception.h>
#include <sstream>

namespace nidas { namespace core {

class SampleLengthException : public nidas::util::Exception {
public:
    SampleLengthException(const std::string& msg,size_t val, size_t maxVal):
        nidas::util::Exception("")
    {
        std::ostringstream os;
        os << "SampleLengthException: " << msg << ": value=" << val <<
            " exceeds maximum allowed value=" << maxVal;
        _what = os.str();
    }
    /**
     * Copy constructor.
     */
    SampleLengthException(const SampleLengthException& e) : Exception(e) {}

    virtual ~SampleLengthException() throw() {}

    virtual SampleLengthException* clone() const {
        return new SampleLengthException(*this);
    }

};

}}	// namespace nidas namespace core

#endif
