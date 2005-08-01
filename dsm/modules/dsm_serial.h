/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate$
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL$

  *******************************************************************

   RTLinux serial driver for NCAR ADS3 DSM system. Serial port
   input is assembled into time-tagged samples.

*/

#ifndef DSM_SERIAL_H
#define DSM_SERIAL_H

#ifndef __RTCORE_KERNEL__
/* User programs need this for the _IO macros, but kernel
 * modules get their's elsewhere.
 */
#include <sys/ioctl.h>
#include <sys/types.h>
typedef size_t rtl_size_t;
#else
#include <sys/rtl_types.h>
#endif 

#include <irigclock.h>		/* for irigClockRates */
#include <dsm_sample.h>

#define DSM_SERIAL_MAGIC 'S'

#define MAX_DSM_SERIAL_MESSAGE_SIZE 2048

struct dsm_serial_prompt {
  char str[128];
  int len;
  enum irigClockRates rate;
};

struct dsm_serial_record_info {
  char sep[16];
  int sepLen;
  unsigned char atEOM;
  int recordLen;
};

struct dsm_serial_status {
    rtl_size_t pe_cnt;
    rtl_size_t oe_cnt;
    rtl_size_t fe_cnt;
    rtl_size_t input_char_overflows;
    rtl_size_t output_char_overflows;
    rtl_size_t sample_overflows;
    rtl_size_t nsamples;
    int char_transmit_queue_length;
    int char_transmit_queue_size;
    int sample_queue_length;
    int sample_queue_size;
};

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */

/*
 * There is a problem with struct termios in RT-linux.  It is defined
 * differently in various header files.  The essential fields
 * at the beginning of the structure are the same, but the value of
 * NCCS varies, and some definitions include other trailing fields.
 * is different.
 *
 * Here is the basic definition, from
 *   /opt/rtldk-2.0/rtlinuxpro/linux/include/asm/termbits.h
 *************************************************************
 *  typedef unsigned char   cc_t;
 *  typedef unsigned int    speed_t;
 *  typedef unsigned int    tcflag_t;

 *  #define NCCS 19
 *  struct termios {
 *    tcflag_t c_iflag;               * input mode flags *
 *    tcflag_t c_oflag;               * output mode flags *
 *    tcflag_t c_cflag;               * control mode flags *
 *    tcflag_t c_lflag;               * local mode flags *
 *    cc_t c_line;                    * line discipline *
 *    cc_t c_cc[NCCS];                * control characters *
 *   };
 *************************************************************

 * FSM labs wrote  /opt/rtldk-2.0/rtlinuxpro/include/posix/termios.h
 * for their serial driver, which added a bunch of stuff after c_cc.

 * glibc comes with /opt/arm_tools/arm-linux/include/bits/termios.h
 * which declares NCCS as 32, and adds some speed fields after c_cc.
 * That is the termios.h that is likely to be included in 
 * user code.

 * As long as we just use the 4 flag fields and the essential
 * values in c_cc we're OK.  However user and driver code may
 * disagree on sizeof(struct termios).

 * The _IO macros use sizeof on their third arg to create the
 * ioctl command value.  To avoid problems we'll just use "int"
 * as the third value, so that user and driver code create the
 * same ioctl values.  Then when doing the DSMSER_TCGETS ioctls,
 * use SIZEOF_TERMIOS for the length, from either user or
 * kernel code.
 */

#define SIZEOF_TERMIOS 36	/* lowest common denominator for termios len */

#define DSMSER_TCSETS _IOW(DSM_SERIAL_MAGIC,0,int)
#define DSMSER_TCGETS _IOR(DSM_SERIAL_MAGIC,1,int)
#define DSMSER_WEEPROM _IO(DSM_SERIAL_MAGIC,2)
#define DSMSER_SET_PROMPT _IOW(DSM_SERIAL_MAGIC,3,\
	struct dsm_serial_prompt)
#define DSMSER_GET_PROMPT _IOR(DSM_SERIAL_MAGIC,4,\
	struct dsm_serial_prompt)

#define DSMSER_START_PROMPTER _IO(DSM_SERIAL_MAGIC,5)
#define DSMSER_STOP_PROMPTER _IO(DSM_SERIAL_MAGIC,6)

#define DSMSER_SET_RECORD_SEP _IOW(DSM_SERIAL_MAGIC,7,\
	struct dsm_serial_record_info)
#define DSMSER_GET_RECORD_SEP _IOR(DSM_SERIAL_MAGIC,8,\
	struct dsm_serial_record_info)
#define DSMSER_GET_STATUS _IOR(DSM_SERIAL_MAGIC,9,\
	struct dsm_serial_status)
#define DSMSER_SET_LATENCY _IOW(DSM_SERIAL_MAGIC,10,\
	long)

#ifdef __RTCORE_KERNEL__

#include <linux/circ_buf.h>
#include <rtl_pthread.h>
#include <rtl_semaphore.h>

/* exposed functions that we provide */

extern int dsm_serial_get_numboards();

extern const char* dsm_serial_get_devprefix();

extern int dsm_serial_get_numports(int board);

extern const char* dsm_serial_get_devname(int port);

struct dsm_sample_circ_buf {
    struct dsm_sample **buf;
    int head;
    int tail;
};

#define SAMPLE_QUEUE_SIZE 16

#define UNKNOWN_TIMETAG_VALUE 0xffffffff

enum boardTypes { BOARD_UNKNOWN, BOARD_WIN_COM8 = 1 };

struct serialPort {
    struct serialBoard* board;	/* pointer to my board structure */
    char *devname;	/* device name */
    int portNum;	/* port number: 0-(N-1), N=total number of ports
    			 * maintained by this driver, counting all boards */
    int portIndex;	/* which port on this board, 0-(N-1), where
    			 * N is the number of ports on the board */
    int ioport;		/* ISA IOport address, e.g. 0x300 */
    unsigned long addr;	/* full address, including BASE address of ISA */
    int irq;		/* port's interrupt */
    int type;		/* type of uart, one of the values from serial.h */
    rtl_spinlock_t lock;	/* mutex lock for the port */
    int flags;		/* used for ASYNC_* flags in serial.h */
    int revision;
    unsigned char MCR;
    unsigned char IER;
    unsigned char LCR;
    unsigned char ACR;
    int baud_base;
    int quot;
    int custom_divisor;
    int timeout;
    int xmit_fifo_size;
    struct termios termios;

    struct dsm_serial_prompt prompt;	/* prompt sent to sensor */
    unsigned char promptOn;		/* are we sending prompts? */

			/* information about how records are separated
			 * in the stream of data from the sensor */
    struct dsm_serial_record_info recinfo;
    int sepcnt;

    struct dsm_sample_circ_buf sample_queue;
    struct dsm_sample* sample;	/* current sample being read */

    unsigned long read_timeout_sec;	/* semaphore timeout in read method */
    unsigned long read_timeout_nsec;	/* semaphore timeout in read method */
    rtl_sem_t sample_sem;

    char* unwrittenp;		/* pointer to remaining sample to be written */
    rtl_ssize_t unwrittenl;		/* length left to be written */

    int incount;
    dsm_sample_time_t bom_timetag;
    unsigned long nsamples;		/* counter of samples through the sys */

    struct circ_buf xmit;

    int pe_cnt;
    int oe_cnt;
    int fe_cnt;
    int input_char_overflows;
    int output_char_overflows;
    int sample_overflows;

};

struct serialBoard {
    int type;
    unsigned long addr;
    int irq;
    struct serialPort* ports;
    int numports;
    rtl_spinlock_t lock;
    int int_mask;
};

#endif	/* __RTCORE_KERNEL__ */

#endif
