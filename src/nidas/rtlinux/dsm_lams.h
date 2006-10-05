/* dsm_lams.h

   Header for the LAMS interface.

   Original Author: Mike Spowart

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

     $LastChangedRevision$
         $LastChangedDate: $
           $LastChangedBy$
                 $HeadURL: $
*/

#ifndef LAMS_DRIVER_H
#define LAMS_DRIVER_H

#include <nidas/rtlinux/ioctl_fifo.h>

#define BOARD_NUM  0
#define N_PORTS    3
#define MAX_BUFFER 512 
#define READ_SIZE  1024

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

#define LAMS_NUM_MAX_NR_DEVS 3 /* maximum number of LAMS cards in sys */
#define LAMS_REGION_SIZE 0x10 /* number of 1-byte registers */


/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define LAMS_MAGIC              'L'
#define LAMS_BASE                0xf7000220

#define FLAGS_OFFSET             0x00
#define DATA_OFFSET              0x02
#define DEBUG_OFFSET             0x04
#define AIR_SPEED_OFFSET         0x06

#define FIFO_EMPTY               0x1
#define FIFO_HALF_FULL           0x2
#define FIFO_FULL                0x4

#define LAMS_PATTERN             0x5555
#define NUM_ARRAYS               128
#define N_LAMS                   3
#define N_CHANNELS               1
enum boardTypes { BOARD_LAMS = 1, BOARD_UNKNOWN };

#ifdef __RTCORE_KERNEL__

typedef unsigned long dsm_sample_timetag_t;  
struct lamsPort {
  dsm_sample_timetag_t timetag; /* timetag of sample */
  unsigned int data[MAX_BUFFER];            /* the data */
  rtl_spinlock_t lock;
};

struct lamsBoard {
    int type;
    unsigned long addr;

    int outfd;
    char * fifoName;

    int irq;
//    struct serialPort* ports;
    int numports;
    rtl_spinlock_t lock;
    int int_mask;
};

#endif /* __RTCORE_KERNEL__ */

/* Structures that are passed via ioctls to/from this driver */
struct lams_set {
  int channel;
};

struct LamsData 
{
  int data[MAX_BUFFER];
  int msec;
};

/*
 * The enumeration of IOCTLs that this driver supports.
 */
#define LAMS_SET         _IOW(LAMS_MAGIC,0, struct lams_set)
#define AIR_SPEED        _IOW(LAMS_MAGIC,1, unsigned int)

#endif /* LAMS_DRIVER_H */
