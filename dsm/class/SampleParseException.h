/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
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
