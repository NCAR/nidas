/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
/*
  Simple filters for use in 16 bit digital sampling.

*/

#ifndef NIDAS_SHORT_FILTERS_H
#define NIDAS_SHORT_FILTERS_H

#include "types.h"

/**
 * Enumeration of supported filter types.
 */
enum nidas_short_filter {
        NIDAS_FILTER_UNKNOWN,
        NIDAS_FILTER_PICKOFF,
        NIDAS_FILTER_BOXCAR,
        NIDAS_FILTER_TIMEAVG,
};

#endif
    
