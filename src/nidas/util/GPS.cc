// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

#include "GPS.h"

#include <stdlib.h>
#include <ctype.h>

bool nidas::util::NMEAchecksumOK(const char* rec,int len)
{
    if (len <= 0) return false;

    const char* eor = rec + len - 1;
    if (*rec == '$') rec++;

    if (*eor == '\0') eor--;    // null termination

    for ( ; eor >= rec && ::isspace(*eor); eor--);  // NL, CR

    // eor should now point to second digit of checksum
    // eor-2 should point to '*'
    if (eor < rec + 2 || *(eor - 2) != '*') return false;

    eor--;  // first digit of checksum
    char* cp;
    char cksum = ::strtol(eor,&cp,16);
    if (cp != eor + 2) return false;    // invalid checksum field length

    char calcsum = 0;
    for ( ; rec < eor-1; ) calcsum ^= *rec++;

    return cksum == calcsum;
}

