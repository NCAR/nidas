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

#include <XMLException.h>
#include <XMLStringConverter.h>

using namespace std;
using namespace dsm;

/*
 * Create an XMLException from a xercesc::XMLException.
 */
dsm::XMLException::XMLException(const xercesc::XMLException& e):
    atdUtil::Exception("XMLException","")
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
dsm::XMLException::XMLException(const xercesc::SAXException& e):
    atdUtil::Exception("XMLException",(const char*) XMLStringConverter(e.getMessage()))
{
}

/**
 * Create an XMLException from a xercesc::DOMException.
 */
dsm::XMLException::XMLException(const xercesc::DOMException& e):
    atdUtil::Exception("XMLException",(const char*) XMLStringConverter(e.getMessage()))
{
}

