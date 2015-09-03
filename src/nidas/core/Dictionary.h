// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_DICTIONARY_H
#define NIDAS_CORE_DICTIONARY_H

#include <string>

namespace nidas { namespace core {

/**
 * Interface for a Dictionary class, which can return a string
 * value for a string token name.
 */
class Dictionary {
public:
    virtual ~Dictionary() {}
    virtual bool getTokenValue(const std::string& token,std::string& value) const = 0;

    /**
     * Utility function that scans a string for tokens like ${XXXX}, or
     * $XXX followed by any characters from ".$/", and replaces them
     * with what is returned by the virtual method getTokenValue(token,value).
     */
    std::string expandString(const std::string& input) const;
};

}}	// namespace nidas namespace core

#endif
