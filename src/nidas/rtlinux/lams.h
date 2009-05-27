/* lams.h

   Header for the LAMS interface.
   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research 2004
   
   Revisions:
     $LastChangedRevision$
         $LastChangedDate$
           $LastChangedBy$
                 $HeadURL$
*/

#ifndef LAMS_DRIVER_H
#define LAMS_DRIVER_H

#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/linux/types.h>

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */
//#define LAMS_NUM_MAX_NR_DEVS 3 // maximum number of LAMS cards in sys
//#define N_PORTS    3
//#define READ_SIZE  1024
#define MAX_BUFFER  1024 

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define LAMS_MAGIC              'L'

#define FLAGS_OFFSET             0x00
#define RAM_CLEAR_OFFSET         0x00
#define AVG_DATA_OFFSET          0x02
#define DATA_OFFSET              0x02
#define PEAK_CLEAR_OFFSET        0x04
#define DEBUG_OFFSET             0x04
#define PEAK_DATA_OFFSET         0x06
#define AIR_SPEED_OFFSET         0x06

#define FIFO_EMPTY               0x1
#define FIFO_HALF_FULL           0x2
#define FIFO_FULL                0x4

//#define LAMS_PATTERN             0x5555
//#define NUM_ARRAYS               128
//#define N_LAMS                   3
#define N_CHANNELS               1

//enum boardTypes { BOARD_LAMS = 1, BOARD_UNKNOWN };

struct lamsPort {
  dsm_sample_time_t timetag;     // timetag of sample
  dsm_sample_length_t size;      // number of bytes in data
  unsigned int data[MAX_BUFFER]; // the data
};
#ifdef __RTCORE_KERNEL__

/*
struct lamsBoard {
    int type;
    unsigned long addr;

    int outfd;
    char * fifoName;

    int irq;
    int numports;
    rtl_spinlock_t lock;
    int int_mask;
};
*/
#endif // __RTCORE_KERNEL__

// Structures that are passed via ioctls to/from this driver
struct lams_set {
  int channel;
};
// The enumeration of IOCTLs that this driver supports.
#define LAMS_SET_CHN     _IOW(LAMS_MAGIC,0, struct lams_set)
#define AIR_SPEED        _IOW(LAMS_MAGIC,1, unsigned int)
#define N_AVG            _IOW(LAMS_MAGIC,2, unsigned int)
#define N_SKIP           _IOW(LAMS_MAGIC,3, unsigned int)
#define CALM             _IOW(LAMS_MAGIC,4, int)

#endif // LAMS_DRIVER_H
