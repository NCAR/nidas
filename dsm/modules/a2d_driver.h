/* a2d_driver.h

   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   Header for test rtl driver.

   Original Author: Grant Gray

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#ifndef A2D_DRIVER_H
#define A2D_DRIVER_H

#include <dsm_sample.h>		// get dsm_sample typedefs

#ifndef __RTCORE_KERNEL__
/* User programs need this for the _IO macros, but kernel
 * modules get their's elsewhere.
 */
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

//Conveniences
#ifndef		US
#define		US	unsigned short
#endif
#ifndef		UL
#define		UL	unsigned long
#endif
#ifndef		UC
#define		UC	unsigned char
#endif
#ifndef		SS
#define		SS	short
#endif
#ifndef		SL
#define		SL	long
#endif
#ifndef		SC
#define		SC	char
#endif

#define	MAXA2DS			8	// Max A/D's per card
#define	A2D_MAX_RATE	5000
#define INTRP_RATE		100
#define	RATERATIO		(A2D_MAX_RATE/INTRP_RATE)	


// A/D Filter configuration file parameters
#define CONFBLOCKS  12  // 12 blocks as described below
#define CONFBLLEN	43	// 42 data words plus 1 CRCC (Confirmed by ADI)

/* Structures that are passed via ioctls to/from this driver */
typedef struct
{
	US a2dstat[MAXA2DS];
	US fifostat;
}A2D_GET;

typedef struct 
{
	int	vcalx8;		// Calibration voltage: 
				// 128=0, 48=-10, 208 = +10, .125 V/bit
	US	status[MAXA2DS];	// A/D status flag
//
	int	gain[MAXA2DS];	// Gain settings 
	int	Hz[MAXA2DS];		// Sample rate in Hz. 0 is off.
	int	calset[MAXA2DS];	// Calibration flags
	int	offset[MAXA2DS];	// Offset flags
	float	norm[MAXA2DS];	// Normalization factors
	US	master;		// Designates master A/D
	US	ctr[MAXA2DS];		// Current value of ctr;
	UL	ptr[MAXA2DS];		// Pointer offset from beginning of 
	US	filter[CONFBLOCKS*CONFBLLEN+1];	// Filter data
				// data summing buffer
} A2D_SET;

typedef struct 
{
	dsm_sample_time_t timestamp;	// timetag of sample 
	dsm_sample_length_t size;		// number of bytes in data 
  	SS data[RATERATIO*MAXA2DS]; 
}A2DSAMPLE;


/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define A2D_MAGIC 'A'

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define A2D_GET_IOCTL _IOR(A2D_MAGIC,0,A2D_GET)
#define A2D_SET_IOCTL _IOW(A2D_MAGIC,1,A2D_SET)
#define A2D_CAL_IOCTL _IOW(A2D_MAGIC,2,A2D_SET)
#define A2D_RUN_IOCTL _IO(A2D_MAGIC,3)
#define A2D_STOP_IOCTL _IO(A2D_MAGIC,4)
#define A2D_RESTART_IOCTL _IO(A2D_MAGIC,5)


#ifdef __RTCORE_KERNEL__
/********  Start of definitions used by the driver module only **********/

#include <dsm_viper.h>		// get SYSTEM_ISA_IOPORT_BASE

#define	HWFIFODEPTH		1024

//Status/error
#define A2DLOADOK	 	 0

#define	ERRA2DNOFILE	-1	//Error opening file
#define	ERRA2DCHAN		-2	//Channel # requested is out of bounds
#define	ERRA2DGAIN		-3	//Gain value out of bounds
#define	ERRA2DVCAL		-4	//Vcal out of bounds
#define	ERRA2DCRC		-5	//Data corrupted
#define	ERRA2DID		-6	//Wrong device ID
#define	ERRA2DCONV		-7	//Conversion data invalid
#define	ERRA2DCHIPID	-8	//Chip ID error
#define	ERRA2DRATE		-9	//A/D sample rate error
#define	ERRA2DCFG		-10	//A/D configuration error

//Card base address for ISA bus
#define	A2DMASTER		7	//A/D chip designated to produce interrupts
#define	A2DIOBASE		0x000003A0
#define	A2DIOSEP		0x00000010	// Card addr separation
#define	A2DBASE			(A2DIOBASE + SYSTEM_ISA_IOPORT_BASE)
#define	A2DIOWIDTH		0x10	// Width of I/O space

// I/O channels for the A/D card
// To point IO at a channel, first load
//   the channel enable latch by writing
//   the channel number to A2DBASE+A2DIOLOAD
//   e.g. *(unsigned short *)(A2DBASE+A2DIOLOAD) = A2DIOFIFO;
//   will point the enable latch at the FIFO output.

#define	A2DIOFIFO		0x0	//FIFO data (read), FIFO Control (write)
#define	A2DIOSTAT		0x1	//A/D status (read), command (write)
#define	A2DSTATRD		0x9	//Same as A2DIOSTAT; BSD3(=A2DRWN) high (rd)
#define	A2DCMNDWR		0x1	//Same as A2DIOSTAT; BSD3(=A2DRWN) low	(wr)
#define A2DIODATA		0x2 // A/D data(read), config(write)
#define	A2DDATARD		0xA	// Same as A2DIODATA; BSD3(=A2DRWN) high (rd)
#define	A2DCONFWR		0x2	// Same as A2DIODATA; BSD3(=A2DRWN) low  (wr)
#define	A2DIOGAIN03		0x3	//A/D chan 0-3 gain read/write
#define	A2DIOGAIN47		0x4	//A/D chan 4-7 gain read/write
#define	A2DIOVCAL		0x5	//VCAL set (DAC ch 0) read/write
#define A2DIOSYSCTL		0x6	//A/D INT lines(read),Cal/offset (write)
#define A2DIOINTRD		0x6	//A/D INT lines(read),Cal/offset (write)
#define	A2DIOFIFOSTAT	0x7	//FIFO stat (read), Set master A/D (write) 
#define	A2DIOLOAD		0xF	//Load A/D configuration data

//A/D Chip command words (See A2DIOSTAT and A2DCMNDWR above)
#define	A2DREADID		0x8802	//Read device ID
#define	A2DREADDATA		0x8d21	//Read converted data
#define	A2DWRCONFIG		0x1800	//Write configuration data
#define	A2DWRCONFEM		0x1A00	//Write configuration, mask data
#define	A2DABORT		0x0000	//Soft reset; still configured
#define A2DBFIR			0x2000	//Boot from internal ROM

//A2D Status register bits
#define	A2DINSTBSY		0x8000	//Instruction being performed
#define	A2DDATARDY		0x4000	//Data ready to be read (Read cycle)
#define	A2DDATAREQ		0x2000	//New data required (Write cycle)
#define	A2DIDERR		0x1000	//Chip ID error
#define	A2DCRCERR		0x0800	//Data corrupted--CRC error
#define	A2DDATAERR		0x0400	//Conversion data invalid
#define	A2DINSTREG15	0x0200	//Instr reg bit	15
#define	A2DINSTREG13	0x0100	//				13
#define	A2DINSTREG12	0x0080	//				12
#define	A2DINSTREG11	0x0040	//				11
#define	A2DINSTREG06	0x0020	//				06
#define	A2DINSTREG05	0x0010	//				05
#define	A2DINSTREG04	0x0008	//				04
#define	A2DINSTREG01	0x0004	//				01
#define	A2DINSTREG00	0x0002	//				00
#define	A2DCONFIGEND	0x0001	//Configuration End Flag.

//FIFO Control Word bit definitions

// A/D Control bits
#define	FIFOCLR			0x01	//Cycle this bit 0-1-0 to clear FIFO
#define	A2DAUTO			0x02	//Set = allow A/D's to run automatically
#define	A2DSYNC			0x04	//Set then cycle A2DSYNCCK to stop A/D's
#define	A2DSYNCCK		0x08	//Cycle to latch A2DSYNC bit value
#define	A2D1PPSEBL		0x10	//Set to allow GPS 1PPS to clear SYNC
#define	FIFODAFAE		0x20	//Set to clamp value of AFAE in FIFO 
#define	A2DSTATEBL		0x40	//Not used. 
#define	FIFOWREBL		0x80	//Enable writing to FIFO.

// FIFO Status bits
#define	FIFOHF			0x01	// FIFO half full
#define FIFOAFAE		0x02	// FIFO almost full/almost empty
#define FIFONOTEMPTY	0x04	// FIFO not empty
#define	FIFONOTFULL		0x08  // FIFO not full
#define INV1PPS			0x10	// Inverted 1 PPS pulse


/*
static int __init a2d_init();		//For Linux kernel
static void __exit a2d_cleanup(void);	//For Linux kernel
*/

#define	RTL_DEBUGIT(a)	rtl_printf("DEBUGIT %d\n", a)
#define	DEBUGIT(a)	printf("DEBUGIT %d\n", a)

#endif

#endif
