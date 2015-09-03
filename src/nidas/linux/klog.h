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
									      
   Handy macros for logging from kernel modules.

*/

#ifndef NIDAS_LINUX_KLOG_H
#define NIDAS_LINUX_KLOG_H

#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/module.h>

#ifdef DEBUG
#define KLOG_DEBUG(fmt,args...)	\
	printk(KERN_DEBUG	"%s: %s: DEBUG: " fmt,__FILE__,__FUNCTION__, ## args)
#else
#define KLOG_DEBUG(fmt,args...)	/* nothing */
#endif

#define KLOG_INFO(fmt,args...)	\
	printk(KERN_INFO	"%s: INFO: " fmt, module_name(THIS_MODULE),## args)
#define KLOG_NOTICE(fmt,args...)	\
	printk(KERN_NOTICE	"%s: NOTICE: " fmt, module_name(THIS_MODULE),## args)
#define KLOG_WARNING(fmt,args...)	\
	printk(KERN_WARNING	"%s: WARNING: " fmt, module_name(THIS_MODULE),## args)
#define KLOG_ERR(fmt,args...)	\
	printk(KERN_ERR 	"%s: ERROR: " fmt, module_name(THIS_MODULE),## args)
#define KLOG_CRIT(fmt,args...)	\
	printk(KERN_CRIT	"%s: CRITICAL: " fmt, module_name(THIS_MODULE),## args)
#define KLOG_ALERT(fmt,args...)	\
	printk(KERN_ALERT	"%s: ALERT: " fmt, module_name(THIS_MODULE),## args)
#define KLOG_EMERG(fmt,args...)	\
	printk(KERN_EMERG	"%s: EMERGENCY: " fmt, module_name(THIS_MODULE),## args)

#endif

#endif
