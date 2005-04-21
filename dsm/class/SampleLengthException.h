/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SAMPLELENGTHEXCEPTION_H
#define DSM_SAMPLELENGTHEXCEPTION_H

#include <sstream>
#include <string>

namespace dsm {

class SampleLengthException : public std::exception {
public:
    SampleLengthException(const std::string& msg,size_t val, size_t maxVal) {
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

}

#endif
