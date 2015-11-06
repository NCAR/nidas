/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
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
    	(string) XMLStringConverter(e.getType()) << ": " << 
    	(string) XMLStringConverter(e.getMessage());
    _what = ost.str();
}

/**
 * Create an XMLException from a xercesc::SAXException.
 */
nidas::core::XMLException::XMLException(const xercesc::SAXException& e):
    n_u::Exception("XMLException",(string) XMLStringConverter(e.getMessage()))
{
}

/**
 * Create an XMLException from a xercesc::DOMException.
 */
nidas::core::XMLException::XMLException(const xercesc::DOMException& e):
    n_u::Exception("XMLException",(string) XMLStringConverter(e.getMessage()))
{
}

