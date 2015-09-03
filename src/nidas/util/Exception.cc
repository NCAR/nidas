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

#include <nidas/util/Exception.h>
// #include <iostream>
#include <cstring>

using std::string;

namespace nidas { namespace util {

string Exception::errnoToString(int err)
{
    if (err == 0) return string();
#ifdef sun
    char *cp = ::strerror(err); 
    return string(cp);
#else
    char msg[256];
    msg[0] = 0;

    /* the linux manpage for strerror_r shows this prototype
     *      int strerror_r(int errnum, char *buf, size_t n);
     * Later in the manpage it mentions the GNU extension:
     *      char *strerror_r(int errnum, char *buf, size_t n);
     * Linux is using the GNU extension, (which does not change buf).
     */

    char* cp = ::strerror_r(err,msg,sizeof msg - 1); 
    // std::cerr << "cp=" << cp << " msg=" << msg << std::endl;
    return string(cp);
#endif
}

}}
