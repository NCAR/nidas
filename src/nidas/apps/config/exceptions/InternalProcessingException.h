/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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
#ifndef _InternalProcessingException_h
#define _InternalProcessingException_h

/*
 * InternalProcessingException
 *  - exception for unexpected internal problems,
 *    e.g. null pointers that should not happen
 */

class InternalProcessingException : public nidas::util::Exception
{
 public:

  InternalProcessingException(const std::string & msg) :
    nidas::util::Exception("InternalProcessingException",msg)
    { }

  InternalProcessingException(const nidas::util::Exception & e) :
    nidas::util::Exception("InternalProcessingException",e.what())
    { }

  InternalProcessingException* clone() const {
    return new InternalProcessingException(*this);
    }

};

#endif
