/* mesa.h

   Header for the Mesa Electronics 4I34 driver.

   Original Author: Mike Spowart

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

     $LastChangedRevision$
         $LastChangedDate: $
           $LastChangedBy$
                 $HeadURL: $
*/

#ifndef MESA_DRIVER_H
#define MESA_DRIVER_H

#define BOARD_NUM  0

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

#define MESA_4I34_MAX_NR_DEVS 4 /* maximum number of Mesa 4I34 cards in sys */
#define MESA_REGION_SIZE 0x10 /* number of 1-byte registers */

/* registers on the Mesa 4I34 starting at the ioport address */
#define R_4I34DATA    0 /* 4I34 data register. */
#define R_4I34CONTROL 1 /* 4I34 control register. */
#define R_4I34STATUS  1 /* 4I34 status register. */

/* Masks for R_4I34CONTROL.
*/
#define B_4I34CFGCS           0 /* Chip select. Enables read/write access to the FPGA via R_4I34DATA. */
#define M_4I34CFGCSON         (0x0 << B_4I34CFGCS) /* Enable access. */
#define M_4I34CFGCSOFF        (0x1 << B_4I34CFGCS) /* Disable access. */

#define B_4I34CFGINIT         1 /* Control programming. */
#define M_4I34CFGINITASSERT   (0x0 << B_4I34CFGINIT) /* Wipe current configuration, prepare for programming. */
#define M_4I34CFGINITDEASSERT (0x1 << B_4I34CFGINIT) /* Wait for programming data. */

#define B_4I34CFGWRITE        2 /* Data direction control for R_4I34DATA. */
#define M_4I34CFGWRITEENABLE  (0x0 << B_4I34CFGWRITE) /* CPU --> R_4I34DATA. */
#define M_4I34CFGWRITEDISABLE (0x1 << B_4I34CFGWRITE) /* R_4I34DATA --> CPU. */

#define B_4I34LED             3 /* Red LED control. */
#define M_4I34LEDON           (0x1 << B_4I34LED)
#define M_4I34LEDOFF          (0x0 << B_4I34LED)

/* Masks for R_4I34STATUS.  */
#define B_4I34PROGDUN 0 /* Programming-done flag. Set when the FPGA "program" has been successfully uploaded to the 4I34. */
#define M_4I34PROGDUN (0x1 << B_4I34PROGDUN)
#define PROGWAITLOOPCOUNT 20000

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define MESA_MAGIC              'M'

#define MESA_BASE                0xf7000220
#define STROBES_OFFSET           0x00
#define HISTOGRAM_CLEAR_OFFSET   0x01
#define HISTOGRAM_READ_OFFSET    0x02
#define HOUSE_ADVANCE_OFFSET     0x03
#define HOUSE_READ_OFFSET        0x04
#define HOUSE_RESET_OFFSET       0x05
#define COUNT0_READ_OFFSET       0x06
#define COUNT1_READ_OFFSET       0x08
#define RADAR_READ_OFFSET        0x0A

/* number of counters, radar altimeters and PMS260X */
#define N_MESA 1
#define N_COUNTERS 2
#define N_RADARS 1
#define N_PMS260X 1

unsigned long filesize;

typedef unsigned long dsm_sample_timetag_t;  
struct dsm_mesa_sample {
  dsm_sample_timetag_t timetag; /* timetag of sample */
  unsigned int data;      /* the data */
};

/* Structures that are passed via ioctls to/from this driver */
struct counters_set {
  int channel;
  int rate;
};
struct radar_set {
  int channel;
  int rate;
};
struct pms260x_set {
  int channel;
  int rate;
};

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define MESA_LOAD        _IOW(MESA_MAGIC,0,int)
#define COUNTERS_SET     _IOW(MESA_MAGIC,1,struct counters_set)
#define RADAR_SET        _IOW(MESA_MAGIC,2,struct radar_set)
#define PMS260X_SET      _IOW(MESA_MAGIC,3,struct pms260x_set)

#include <ioctl_fifo.h>

#ifdef __KERNEL__

#endif

#endif
