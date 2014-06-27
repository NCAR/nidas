// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

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
 * Replace all occurences of pat in string in with rep.
 */
extern void replaceCharsIn(std::string& in,const std::string& pat, const std::string& rep);

extern std::string replaceChars(const std::string& in,const std::string& pat, const std::string& rep);

/**
 * Run "svn status -v --depth empty" on a path and return a concatentated
 * string of revision + flags, where flags are the first 8 characters.
 */
extern std::string svnStatus(const std::string& path) throw (IOException);

}}	// namespace nidas namespace core

#endif
