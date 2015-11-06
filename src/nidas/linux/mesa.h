/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/* mesa.h

   Header for the Mesa Electronics 4I34 driver.

   Original Author: Mike Spowart

*/

#ifndef _mesa_driver_h_
#define _mesa_driver_h_

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

#include <nidas/linux/types.h>
#include <nidas/linux/util.h>
#include <nidas/linux/irigclock.h>

// Sample ID's
#define ID_COUNTERS	1
#define ID_DIG_IN	2
#define ID_DIG_OUT	3
#define ID_260X		4
#define ID_RADAR	5

#define MESA_MAX_FPGA_BUFFER	512

#define TWO_SIXTY_BINS	64

// number of counters, radar altimeters and PMS260X
#define N_COUNTERS	2
#define N_RADARS	1
#define N_PMS260X	1

// Structures that are passed via ioctls to/from this driver
struct counters_set
{
        int nChannels;
        int rate;
};
struct radar_set
{
        int nChannels;
        int rate;
};
struct pms260x_set
{
        int nChannels;
        int rate;
};

struct digital_in
{
        int nChannels;
        int value;
};

struct mesa_prog
{
        int len;
        char buffer[MESA_MAX_FPGA_BUFFER];
};

struct mesa_status
{
        unsigned int missedSamples;
};

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define MESA_MAGIC		'M'

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define MESA_LOAD_START	_IO(MESA_MAGIC, 0)
#define MESA_LOAD_BLOCK	_IOW(MESA_MAGIC, 1, struct mesa_prog)
#define MESA_LOAD_DONE	_IO(MESA_MAGIC, 2)
#define COUNTERS_SET	_IOW(MESA_MAGIC, 3, struct counters_set)
#define RADAR_SET	_IOW(MESA_MAGIC, 4, struct radar_set)
#define PMS260X_SET	_IOW(MESA_MAGIC, 5, struct pms260x_set)
#define DIGITAL_IN_SET	_IOW(MESA_MAGIC, 6, struct digital_in)
#define	MESA_STOP	_IO(MESA_MAGIC, 7)
#define MESA_IOC_MAXNR 7

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/wait.h>

#define MESA_4I34_MAX_NR_DEVS	4       // maximum number of Mesa 4I34 cards in sys
#define MESA_REGION_SIZE	16    // # of reserved ioport addresses

// registers on the Mesa 4I34 starting at the ioport address
#define R_4I34DATA    0         // 4I34 data register.
#define R_4I34CONTROL 1         // 4I34 control register.
#define R_4I34STATUS  1         // 4I34 status register.

/* Masks for R_4I34CONTROL.
*/
#define B_4I34CFGCS           0 /* Chip select. Enables read/write access to the FPGA via R_4I34DATA. */
#define M_4I34CFGCSON         (0x0 << B_4I34CFGCS)      /* Enable access. */
#define M_4I34CFGCSOFF        (0x1 << B_4I34CFGCS)      /* Disable access. */

#define B_4I34CFGINIT         1 /* Control programming. */
#define M_4I34CFGINITASSERT   (0x0 << B_4I34CFGINIT)    /* Wipe current configuration, prepare for programming. */
#define M_4I34CFGINITDEASSERT (0x1 << B_4I34CFGINIT)    /* Wait for programming data. */

#define B_4I34CFGWRITE        2 /* Data direction control for R_4I34DATA. */
#define M_4I34CFGWRITEENABLE  (0x0 << B_4I34CFGWRITE)   /* CPU --> R_4I34DATA. */
#define M_4I34CFGWRITEDISABLE (0x1 << B_4I34CFGWRITE)   /* R_4I34DATA --> CPU. */

#define B_4I34LED             3 /* Red LED control. */
#define M_4I34LEDON           (0x1 << B_4I34LED)
#define M_4I34LEDOFF          (0x0 << B_4I34LED)

/* Masks for R_4I34STATUS.  */
#define B_4I34PROGDUN		0       /* Programming-done flag. Set when the FPGA "program" has been successfully uploaded to the 4I34. */
#define M_4I34PROGDUN		(0x1 << B_4I34PROGDUN)
#define PROGWAITLOOPCOUNT	20000


#define STROBES_OFFSET         0x02     // "0010" (base address) = Read 260X total strobes
#define HISTOGRAM_CLEAR_OFFSET 0x03     // "0011" clear histogram and clear histogram bin index pointer
#define HISTOGRAM_READ_OFFSET  0x04     // "0100" read 260X histogram at bin pointed to by index
#define HOUSE_ADVANCE_OFFSET   0x05     // "0101" 260X housekeeping advance
#define HOUSE_READ_OFFSET      0x06     // "0110" 260X housekeeping data
#define HOUSE_RESET_OFFSET     0x07     // "0111" 260X housekeeping reset
#define COUNT0_READ_OFFSET     0x08     // "1000" Pulse counter #0
#define COUNT1_READ_OFFSET     0x0A     // "1010" Pulse counter #1
#define RADAR_READ_OFFSET      0x0C     // "1100" Altitude data
#define TWOSIXTY_RESETS_OFFSET 0x0E     // "1110" 260X Reset line.

struct radar_state
{
        dsm_sample_time_t timetag;
        unsigned short prevData;
        int ngood;
        int npoll;
        int NPOLL;
};

struct MESA_Board
{
        unsigned long addr;     // Base address of board
        char devName[64];
        long latencyJiffies;	// buffer latency in jiffies
        unsigned long lastWakeup;   // when were read & poll methods last woken

        wait_queue_head_t rwaitq;       // wait queue for user reads

        struct irig_callback *cntrCallback;
        struct irig_callback *radarCallback;
        struct irig_callback *p260xCallback;

        struct dsm_sample_circ_buf cntr_samples;  
        struct dsm_sample_circ_buf radar_samples;  
        struct dsm_sample_circ_buf p260x_samples;  

        struct sample_read_state cntr_read_state;
        struct sample_read_state radar_read_state;
        struct sample_read_state p260x_read_state;

        int nCounters;
        int nRadars;
        int n260X;
        unsigned int progNbytes;

        struct radar_state rstate;

        struct mesa_status status;

        // available for open. Used to enforce exclusive access
        atomic_t available;                    
};

#endif                          /* __KERNEL__ */

#endif
