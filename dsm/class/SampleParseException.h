/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SAMPLEPARSEEXCEPTION_H
#define DSM_SAMPLEPARSEEXCEPTION_H

#include <string>
#include <atdUtil/Exception.h>

namespace dsm {

class SampleParseException : public atdUtil::Exception {

  public:
 
    /**
     * Create a ParseException, passing a name of what you're trying
     * to parse, a message, and the line number of the document.
     */
    SampleParseException(const std::string& what,
	const std::string& msg):
      atdUtil::Exception("SampleParseException", what + ": " + msg) {}

    /**
     * Create an ParseException, passing a message.
     */
    SampleParseException(const std::string& message):
      atdUtil::Exception(message) {}

    /**
     * clone myself (a "virtual" constructor).
     */
    SampleParseException* clone() const {
      return new SampleParseException(*this);
    }
  };
}

#endif
