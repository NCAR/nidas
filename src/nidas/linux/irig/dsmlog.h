/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $
									      
  $LastChangedRevision: 3648 $
									      
  $LastChangedBy: cjw $
									      
  $HeadURL: http://svn.atd.ucar.edu/svn/nids/trunk/src/nidas/rtlinux/dsmlog.h $

  *******************************************************************

   Handy macros for logging from Linux modules.

*/

#ifndef DSMLOG_H
#define DSMLOG_H

#include <linux/kernel.h>

#define DSMLOG_DEBUG(fmt,args...)	\
	printk(KERN_DEBUG	"%s: %s: " fmt,__FILE__,__FUNCTION__, ## args)
#define DSMLOG_INFO(fmt,args...)	\
	printk(KERN_INFO	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_NOTICE(fmt,args...)	\
	printk(KERN_NOTICE	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_WARNING(fmt,args...)	\
	printk(KERN_WARNING	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_ERR(fmt,args...)	\
	printk(KERN_ERR	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_CRIT(fmt,args...)	\
	printk(KERN_CRIT	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_ALERT(fmt,args...)	\
	printk(KERN_ALERT	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_EMERG(fmt,args...)	\
	printk(KERN_EMERG	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)

#endif
