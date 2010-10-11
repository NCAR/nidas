/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate$
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL$

  *******************************************************************

   Handy macros for logging from kernel modules.

*/

#ifndef NIDAS_LINUX_KLOG_H
#define NIDAS_LINUX_KLOG_H

#ifdef __KERNEL__

#include <linux/kernel.h>

#ifdef DEBUG
#define KLOG_DEBUG(fmt,args...)	\
	printk(KERN_DEBUG	"%s: %s: " fmt,__FILE__,__FUNCTION__, ## args)
#else
#define KLOG_DEBUG(fmt,args...)	/* nothing */
#endif

#define KLOG_INFO(fmt,args...)	\
	printk(KERN_INFO	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define KLOG_NOTICE(fmt,args...)	\
	printk(KERN_NOTICE	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define KLOG_WARNING(fmt,args...)	\
	printk(KERN_WARNING	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define KLOG_ERR(fmt,args...)	\
	printk(KERN_ERR	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define KLOG_CRIT(fmt,args...)	\
	printk(KERN_CRIT	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define KLOG_ALERT(fmt,args...)	\
	printk(KERN_ALERT	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define KLOG_EMERG(fmt,args...)	\
	printk(KERN_EMERG	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)

#endif

#endif
