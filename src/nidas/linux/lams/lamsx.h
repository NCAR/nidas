/* lams.h

   Header for the LAMS interface.
   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research 2004
   
   Revisions:
     $LastChangedRevision: 5361 $
         $LastChangedDate: 2010-02-19 16:52:59 -0700 (Fri, 19 Feb 2010) $
           $LastChangedBy: cjw $
                 $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/rtlinux/lams.h $
*/

#ifndef NIDAS_LAMS_LAMSX_H
#define NIDAS_LAMS_LAMSX_H

#include <nidas/linux/types.h>

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */
#define SIZE_LAMS_BUFFER  512 
#define MAX_BUFFER  SIZE_LAMS_BUFFER

//#define LAMS_PATTERN             0x5555
//#define NUM_ARRAYS               128
//#define N_LAMS                   3

#ifdef __KERNEL__
struct lams_sample {
       dsm_sample_time_t timetag;       // timetag of sample
       dsm_sample_length_t length;        // number of bytes in data
       unsigned int type;
       unsigned int data[SIZE_LAMS_BUFFER];   // the averages
       unsigned short peak[SIZE_LAMS_BUFFER]; // the peaks
};
#else
// For user-space programs, the data portion of a sample
struct lams_sample {
       unsigned int type;
       unsigned int data[SIZE_LAMS_BUFFER];   // the averages
       unsigned short peak[SIZE_LAMS_BUFFER]; // the peaks
};
#endif


struct lams_status {
        unsigned int missedISRSamples;
        unsigned int missedOutSamples;
};

/** not needed in new driver. */
struct lams_set {
        int channel;
};

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define LAMS_MAGIC              'L'

// The enumeration of IOCTLs that this driver supports.
#define LAMS_SET_CHN     _IOW(LAMS_MAGIC,0, struct lams_set)    // not needed in new driver
#define LAMS_N_AVG       _IOW(LAMS_MAGIC,1, unsigned int)       // put navg and npeak in a struct
#define LAMS_N_PEAKS     _IOW(LAMS_MAGIC,2, unsigned int)
#define LAMS_GET_STATUS  _IOR(LAMS_MAGIC,3, struct lams_status)
#define LAMS_TAS_BELOW   _IO(LAMS_MAGIC,5)
#define LAMS_TAS_ABOVE   _IO(LAMS_MAGIC,6)
#define LAMS_IOC_MAXNR  6

#endif // NIDAS_LAMS_LAMSX_H
