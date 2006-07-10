/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <sstream>

#include <nidas/core/XMLException.h>
#include <nidas/core/XMLStringConverter.h>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

/*
 * Create an XMLException from a xercesc::XMLException.
 */
nidas::core::XMLException::XMLException(const xercesc::XMLException& e):
    n_u::Exception("XMLException","")
{
    ostringstream ost;
    ost << e.getSrcFile() << ", line " << e.getSrcLine() << ": " <<
    	(const char*) XMLStringConverter(e.getType()) << ": " << 
    	(const char*) XMLStringConverter(e.getMessage());
    _what = ost.str();
}

/**
 * Create an XMLException from a xercesc::SAXException.
 */
nidas::core::XMLException::XMLException(const xercesc::SAXException& e):
    n_u::Exception("XMLException",(const char*) XMLStringConverter(e.getMessage()))
{
}

/**
 * Create an XMLException from a xercesc::DOMException.
 */
nidas::core::XMLException::XMLException(const xercesc::DOMException& e):
    n_u::Exception("XMLException",(const char*) XMLStringConverter(e.getMessage()))
{
}

