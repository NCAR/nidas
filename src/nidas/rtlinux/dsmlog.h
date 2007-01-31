/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate$
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL$

  *******************************************************************

   Handy macros for logging from RTL modules.

*/

#ifndef DSMLOG_H
#define DSMLOG_H

#ifdef __RTCORE_KERNEL__

#include <linux/kernel.h>

#define DSMLOG_DEBUG(fmt,args...)	\
	rtl_printf(KERN_DEBUG	"%s: %s: " fmt,__FILE__,__FUNCTION__, ## args)
#define DSMLOG_INFO(fmt,args...)	\
	rtl_printf(KERN_INFO	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_NOTICE(fmt,args...)	\
	rtl_printf(KERN_NOTICE	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_WARNING(fmt,args...)	\
	rtl_printf(KERN_WARNING	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_ERR(fmt,args...)	\
	rtl_printf(KERN_ERR	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_CRIT(fmt,args...)	\
	rtl_printf(KERN_CRIT	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_ALERT(fmt,args...)	\
	rtl_printf(KERN_ALERT	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)
#define DSMLOG_EMERG(fmt,args...)	\
	rtl_printf(KERN_EMERG	"%s: %s: " fmt, __FILE__,__FUNCTION__,## args)

#endif

#endif
