/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
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
   macros for  use across different version of Linux.

   Original Author: Gordon Maclean

*/

#ifndef NIDAS_LINUX_NIDAS_VER_MACROS_H
#define NIDAS_LINUX_NIDAS_VER_MACROS_H

#if defined(__KERNEL__)

#include <linux/version.h>

/**
 * Linux version independent device_create.
 * drvdata in versions >= 2.6.26 is passed as NULL.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
    #define device_create_x(a, b, c, d, ...) device_create(a, b, c, NULL, d, ##__VA_ARGS__)
#else
    #define device_create_x(a, b, c, d, ...) device_create(a, b, c, d, ##__VA_ARGS__)
#endif

#endif
#endif
