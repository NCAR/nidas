/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_SAMPLEPARSEEXCEPTION_H
#define NIDAS_CORE_SAMPLEPARSEEXCEPTION_H

#include <string>
#include <nidas/util/Exception.h>

namespace nidas { namespace core {

class SampleParseException : public nidas::util::Exception
{

  public:
 
    /**
     * Create a ParseException, passing a name of what you're trying
     * to parse, a message, and the line number of the document.
     */
    SampleParseException(const std::string& what,
	const std::string& msg):
      nidas::util::Exception("SampleParseException", what + ": " + msg) {}

    /**
     * Create an ParseException, passing a message.
     */
    SampleParseException(const std::string& message):
      nidas::util::Exception(message) {}

    /**
     * clone myself (a "virtual" constructor).
     */
    SampleParseException* clone() const {
	return new SampleParseException(*this);
    }
};

}}	// namespace nidas namespace core

#endif
