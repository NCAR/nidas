/* mesa.h

   Header for the Mesa Electronics 4I34 driver.

   Original Author: Mike Spowart

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

     $LastChangedRevision$
         $LastChangedDate$
           $LastChangedBy$
                 $HeadURL$
*/

#ifndef _mesa_driver_h_
#define _mesa_driver_h_

#include <nidas/core/dsm_sample.h>              // get dsm_sample typedefs
#include <nidas/rtlinux/irigclock.h>

typedef unsigned short dsm_sample_id_t;

#define MAX_BUFFER	1024
#define READ_SIZE	1000

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

#define MESA_4I34_MAX_NR_DEVS	4	// maximum number of Mesa 4I34 cards in sys
#define MESA_REGION_SIZE	0x12	// number of 1-byte registers

// registers on the Mesa 4I34 starting at the ioport address
#define R_4I34DATA    0		// 4I34 data register.
#define R_4I34CONTROL 1		// 4I34 control register.
#define R_4I34STATUS  1		// 4I34 status register.

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
#define B_4I34PROGDUN		0 /* Programming-done flag. Set when the FPGA "program" has been successfully uploaded to the 4I34. */
#define M_4I34PROGDUN		(0x1 << B_4I34PROGDUN)
#define PROGWAITLOOPCOUNT	20000

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define MESA_MAGIC		'M'

#define MESA_BASE 0x220

#define STROBES_OFFSET         0x02 // "0010" (base address) = Read 260X total strobes
#define HISTOGRAM_CLEAR_OFFSET 0x03 // "0011" clear histogram and clear histogram bin index pointer
#define HISTOGRAM_READ_OFFSET  0x04 // "0100" read 260X histogram at bin pointed to by index
#define HOUSE_ADVANCE_OFFSET   0x05 // "0101" 260X housekeeping advance
#define HOUSE_READ_OFFSET      0x06 // "0110" 260X housekeeping data
#define HOUSE_RESET_OFFSET     0x07 // "0111" 260X housekeeping reset
#define COUNT0_READ_OFFSET     0x08 // "1000" Pulse counter #0
#define COUNT1_READ_OFFSET     0x0A // "1010" Pulse counter #1
#define RADAR_READ_OFFSET      0x0C // "1100" Altitude data
#define TWOSIXTY_READ_OFFSET   0x0E // "1110" read 260X histogram data

// Sample ID's
#define ID_COUNTERS	1
#define ID_DIG_IN	2
#define ID_DIG_OUT	3
#define ID_260X		4
#define ID_RADAR	5

// number of counters, radar altimeters and PMS260X
#define N_MESA_DEVICES	1
#define N_COUNTERS	2
#define N_RADARS	1
#define N_PMS260X	1

#define MAX_SAMPLES	2

#define N_PORTS		(N_COUNTERS+N_RADARS+N_PMS260X)

#define TWO_SIXTY_BINS	64

typedef struct
{
  dsm_sample_time_t timetag;	// timetag of sample
  dsm_sample_length_t size;	// number of bytes in data
  dsm_sample_id_t sampleID;	// Sample ID of this data.
  unsigned short data[MAX_SAMPLES];		// the data
} MESA_SIXTEEN_BIT_SAMPLE;

typedef struct
{
  dsm_sample_time_t timetag;	// timetag of sample
  dsm_sample_length_t size;	// number of bytes in data
  dsm_sample_id_t sampleID;	// Sample ID of this data.
  unsigned short strobes;	// Total strobes.
  unsigned short house[8];	// housekeeping
  unsigned short data[TWO_SIXTY_BINS];	// the data
} MESA_TWO_SIXTY_X_SAMPLE;

// Structures that are passed via ioctls to/from this driver
struct _prog {
  int len;
  char buffer[MAX_BUFFER];
};

struct digital_in {
  int nChannels;
  int value;
};
struct counters_set {
  int nChannels;
  int rate;
};
struct radar_set {
  int nChannels;
  int rate;
};
struct pms260x_set {
  int nChannels;
  int rate;
};


struct MESA_Board
{
  int irq;			// requested IRQ ... are we using this?
  unsigned long addr;		// Base address of board

  int outfd;
  char * fifoName;

  enum irigClockRates counter_rate;
  enum irigClockRates radar_rate;
  enum irigClockRates twoSixty_rate;

  int	nCounters;
  int	nRadars;
  int	n260X;
  size_t progNbytes;

  struct ioctlHandle * ioctlhandle;
};


/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define MESA_LOAD_START	_IO(MESA_MAGIC, 0)
#define MESA_LOAD_BLOCK	_IOW(MESA_MAGIC, 1, struct _prog)
#define MESA_LOAD_DONE	_IO(MESA_MAGIC, 2)
#define COUNTERS_SET	_IOW(MESA_MAGIC, 3, struct counters_set)
#define RADAR_SET	_IOW(MESA_MAGIC, 4, struct radar_set)
#define PMS260X_SET	_IOW(MESA_MAGIC, 5, struct pms260x_set)
#define DIGITAL_IN_SET	_IOW(MESA_MAGIC, 6, struct digital_in)
#define	MESA_STOP	_IO(MESA_MAGIC, 7)

#include <nidas/rtlinux/ioctl_fifo.h>

#ifdef __RTCORE_KERNEL__

#endif

#endif
