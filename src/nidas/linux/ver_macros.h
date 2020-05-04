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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
#define mutex_init(x)               init_MUTEX(x)
#define mutex_lock_interruptible(x) ( down_interruptible(x) ? -ERESTARTSYS : 0)
#define mutex_unlock(x)             up(x)
#endif

/**
 * Linux version independent device_create.
 * drvdata in versions >= 2.6.26 is passed as NULL.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
    #define device_create_x(a, b, c, d, ...) device_create(a, b, c, NULL, d, ##__VA_ARGS__)
#else
    #define device_create_x(a, b, c, d, ...) device_create(a, b, c, d, ##__VA_ARGS__)
#endif

/**
 * Linux 5.0 changed the number of parms for access_ok from 3 to 2.
 * Redhat decided to backport that to RHEL8.1, which is kernel 4.18.
 * Some peoples children...
 */
#ifdef RHEL_RELEASE_CODE
#  if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8,1)
#    define portable_access_ok(mode, userptr, len) access_ok(mode, userptr, len)
#  else
#    define portable_access_ok(mode, userptr, len) access_ok(userptr, len)
#  endif
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
#  define portable_access_ok(mode, userptr, len) access_ok(mode, userptr, len)
#else
#  define portable_access_ok(mode, userptr, len) access_ok(userptr, len)
#endif

#endif
#endif
