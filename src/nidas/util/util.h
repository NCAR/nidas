// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

/*! \namespace nidas::util
    \brief General utility classes.
    nidas::util contains classes of general utility, like Socket, Thread, etc.
    The classes use only the standard Unix system libraries and libstdc++,
    and have no dependency on other external packages such as
    an XML parser, or a logging package.
 */

#ifndef NIDAS_UTIL_UTIL_H
#define NIDAS_UTIL_UTIL_H

#include <string>

#include "IOException.h"

namespace nidas { namespace util {

/**
 * Utility function for replacing backslash sequences in a string.
 *  \\n=newline, \\r=carriage-return, \\t=tab, \\\\=backslash
 *  \\xhh=hex, where hh are (exactly) two hex digits and
 *  \\000=octal, where 000 are exactly three octal digits.
 */
extern std::string replaceBackslashSequences(const std::string& str);

/* note that the above back slashes above are doubled so that
 * doxygen displays them as one back slash.  One does
 * not double them in the parameter string.
 */

/**
 * Utility function for substituting backslash sequences back
 * into a string.
 */
extern std::string addBackslashSequences(const std::string& str);

/**
 * Utility to remove white space characters (matching isspace()) from end of string.
 */
extern void trimString(std::string& str);

/**
 * Replace all occurrences of pat in string in with rep.
 */
extern void replaceCharsIn(std::string& in,const std::string& pat, const std::string& rep);

extern std::string replaceChars(const std::string& in,const std::string& pat, const std::string& rep);

/**
 * Run "svn status -v --depth empty" on a path and return a concatenated
 * string of revision + flags, where flags are the first 8 characters.
 */
extern std::string svnStatus(const std::string& path) throw (IOException);

/*
 *  Test whether a numerical value is between two limits
 */
#define RANGE_CHECK_INC(min, val, max) ((min) <= (val) && (val) <= (max))
#define RANGE_CHECK_EXC(min, val, max) ((min) < (val) && (val) < (max))

/*
 *  Check for characters which are not printable. Generally speaking, this means that
 *  the baud/parity/databits/stopbits are not set up right for sensor comm or the
 *  sensor normal runtime outputs are binary and it has not been put in config mode.
 */
bool isNonPrintable(const char c, bool allowSTXETX);
bool containsNonPrintable(char const * buf, std::size_t len, bool allowSTXETX=false);


}}	// namespace nidas namespace util

#endif
