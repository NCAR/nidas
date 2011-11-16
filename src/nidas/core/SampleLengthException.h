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

#include <sstream>
#include <string>

namespace nidas { namespace core {

class SampleLengthException : public std::exception {
public:
    SampleLengthException(const std::string& msg,size_t val, size_t maxVal):
        _what()
    {
	std::ostringstream os;
	os << msg << ": value=" << val <<
	    " exceeds maximum allowed value=" << maxVal;
	_what = os.str();
    }
    /**
     * Copy constructor.
     */
    SampleLengthException(const SampleLengthException& e) : _what(e._what) {}

    virtual ~SampleLengthException() throw() {}

    virtual SampleLengthException* clone() const {
	return new SampleLengthException(*this);
    }

    virtual const char* what() const throw() { return _what.c_str(); }

private:
    std::string _what;
};

}}	// namespace nidas namespace core

#endif
