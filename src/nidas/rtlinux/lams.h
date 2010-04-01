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

#include <nidas/linux/types.h>

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */
//#define LAMS_NUM_MAX_NR_DEVS 3 // maximum number of LAMS cards in sys
//#define N_PORTS    3
//#define READ_SIZE  1024
//
#define LAMS_SPECTRA_SIZE  512 

//#define LAMS_PATTERN             0x5555
//#define NUM_ARRAYS               128
//#define N_LAMS                   3
#define N_CHANNELS               1

//enum boardTypes { BOARD_LAMS = 1, BOARD_UNKNOWN };

struct lamsPort {
  dsm_sample_time_t timetag;       // timetag of sample
  dsm_sample_length_t size;        // number of bytes in data
  unsigned int avrg[LAMS_SPECTRA_SIZE];   // the averages
  unsigned short peak[LAMS_SPECTRA_SIZE]; // the peaks
};

#ifdef __RTCORE_KERNEL__
struct lams_sample {
       dsm_sample_time_t timetag;       // timetag of sample
       dsm_sample_length_t length;        // number of bytes in data
       unsigned int type;
       unsigned int specAvg[LAMS_SPECTRA_SIZE];   // the averages
       unsigned short specPeak[LAMS_SPECTRA_SIZE]; // the peaks
};
#else
// For user-space programs, the data portion of a sample
struct lams_sample {
       unsigned int type;
       unsigned int specAvg[LAMS_SPECTRA_SIZE];   // the averages
       unsigned short specPeak[LAMS_SPECTRA_SIZE]; // the peaks
};
struct lams_avg_sample {
       unsigned int type;
       unsigned int data[LAMS_SPECTRA_SIZE];   // the averages
};
struct lams_peak_sample {
       unsigned int type;
       unsigned short data[LAMS_SPECTRA_SIZE]; // the peaks
};
#endif

#ifdef __RTCORE_KERNEL__

/*
struct lamsBoard {
    int type;
    unsigned int addr;

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

struct lams_status {
        unsigned int missedISRSamples;
        unsigned int missedOutSamples;
};

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define LAMS_MAGIC              'L'

// The enumeration of IOCTLs that this driver supports.
#define LAMS_SET_CHN     _IOW(LAMS_MAGIC,0, struct lams_set)
#define LAMS_N_AVG            _IOW(LAMS_MAGIC,1, unsigned int)
#define LAMS_N_PEAKS          _IOW(LAMS_MAGIC,2, unsigned int)
#define LAMS_GET_STATUS  _IOR(LAMS_MAGIC,3, struct lams_status)
#define LAMS_TAS_BELOW         _IO(LAMS_MAGIC,5)
#define LAMS_TAS_ABOVE         _IO(LAMS_MAGIC,6)

#ifdef __RTCORE_KERNEL__
#include <nidas/rtlinux/ioctl_fifo.h>
#endif

#endif // LAMS_DRIVER_H
