/* a2d_driver.h

   Time-stamp: <Sun 25-Oct-2004 12:48:04 pm>

   Header for test rtl driver.

   Original Author: Grant Gray

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

*/

#ifndef A2D_DRIVER_H
#define A2D_DRIVER_H


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

#define	A2D_MAX_RATE		10000
#define INTRP_RATE		100
#define	HWFIFODEPTH		1024

//Status/error
#define A2DLOADOK	 	 0

#define	ERRA2DNOFILE		-1	//Error opening file
#define	ERRA2DCHAN		-2	//Channel # requested is out of bounds
#define	ERRA2DGAIN		-3	//Gain value out of bounds
#define	ERRA2DVCAL		-4	//Vcal out of bounds
#define	ERRA2DCRC		-5	//Data corrupted
#define	ERRA2DID		-6	//Wrong device ID
#define	ERRA2DCONV		-7	//Conversion data invalid
#define	ERRA2DCHIPID		-8	//Chip ID error
#define	ERRA2DRATE		-9	//A/D sample rate error

//Card base address for ISA bus
#define	A2DMASTER		7
#define	A2DIOBASE		0x000003A0
#define	A2DIOSEP		0x00000010	// Card addr separation
#define	ARMISABASE		0xf7000000
#define	A2DBASE			(A2DIOBASE+ARMISABASE)
#define	MAXA2DS			8	// Max A/D's per card
#define	A2DIOWIDTH		0x10	// Width of I/O space

// I/O channels for the A/D card
// To point IO at a channel, first load
//   the channel enable latch by writing
//   the channel number to A2DBASE+A2DIOLOAD
//   e.g. *(unsigned short *)(A2DBASE+A2DIOLOAD) = A2DIOFIFO;
//   will point the enable latch at the FIFO output.

#define	A2DIOFIFO		0	//FIFO read, FIFO Control (write)
#define	A2DIOSTAT		1	//A/D status (read), command (write)
#define	A2DIODATA		2	//A/D data(read), config(write)	
#define	A2DIOGAIN03		3	//A/D chan 0-3 gain read/write
#define	A2DIOGAIN47		4	//A/D chan 4-7 gain read/write
#define	A2DIOVCAL		5	//VCAL set (DAC ch 0) read/write
#define A2DIOSYSCTL		6	//A/D INT lines(read),Cal/offset (write)
#define	A2DIOFIFOSTAT		7	//FIFO stat (read), 
#define	A2DIOLOAD		8	//Load A/D configuration data

//A/D Chip command words
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
#define	A2DINSTREG15		0x0200	//Instruction register bit  	15
#define	A2DINSTREG13		0x0100	//				13
#define	A2DINSTREG12		0x0080	//				12
#define	A2DINSTREG11		0x0040	//				11
#define	A2DINSTREG06		0x0020	//				06
#define	A2DINSTREG05		0x0010	//				05
#define	A2DINSTREG04		0x0008	//				04
#define	A2DINSTREG01		0x0004	//				01
#define	A2DINSTREG00		0x0002	//				00
#define	A2DCONFIGEND		0x0001	//Configuration End Flag.

//FIFO Control Word bit definitions
#define	FIFOCLR		0x01	//Cycle this bit 0-1-0 to clear FIFO
#define	A2DAUTO		0x02	//Set = allow A/D's to run automatically
#define	A2DSYNC		0x04	//Set then cycle A2DSYNCCK to stop A/D's
#define	A2DSYNCCK	0x08	//Cycle to latch A2DSYNC bit value
#define	A2D1PPSEBL	0x10	//Set to allow GPS 1PPS to clear SYNC
#define	FIFODAFAE	0x20	//Set to clamp value of AFAE in FIFO 
#define	FIFOSTATEBL	0x40	//Not used. 
#define	A2DRW		0x80	//Enables A/D read operations

// FIFO Status bits
#define	FIFOHF		0x0001	// FIFO half full
#define FIFOAFAE	0x0002	// FIFO almost full/almost empty
#define FIFONOTEMPTY	0x0004	// FIFO not empty
#define	FIFOFULL	0x0008  // FIFO full

/* Structures that are passed via ioctls to/from this driver */
typedef struct 
{
	long timestamp;
	long size;
	long spare;
  	US   inbuf[2*INTRP_RATE*MAXA2DS]; //8 chans*100samples/chan*2bytes/sample
}A2DSAMPLE;

typedef struct
{
	long timestamp;
	long size;
	long spare;
	US a2dstat[8];
	US fifostat;
	char c[2*INTRP_RATE*MAXA2DS];
}A2D_GET;

typedef struct 
{
	int	gain[8];	// Gain settings 
	int	vcalx8;		// Calibration voltage: 
				// 128=0, 48=-10, 208 = +10, .125 V/bit
	double	norm[8];	// Normalization factor
//
	UC	calset;		// Calibration flags
	UC	offset;		// Offset flags
//
	US	filter[2048];	// Filter data
	US	master;		// Designates master A/D
	US	Hz[8];		//Sample rate in Hz. 0 is off.
	US	ctr[8];		// Current value of ctr;
	US	flag[8];	// Address bump flag
	US	status[8];	// A/D status flag
//
	UL	ptr[8];		// Pointer offset from beginning of 
				// data summing buffer
}A2D_SET;


//Function templates
void A2DGetDataSim(void);	// Data simulator for running w/o irig
int  A2DFIFOEmpty(void);	// Tests FIFO empty flag
void A2DDataSim(A2D_SET *a);	// Data simulator--inf loop
int  A2DSetup(A2D_SET *a);
void A2DGetData();		//Read hardware fifo
void A2DPtrInit(A2D_SET *a);	//Initialize pointer to data areas
void A2DChSel(int a);		//Select the A/D board channel
US   A2DStatus(int a);		//Get A/D status
void A2DCommand(int a, US b);	//Issue command b to A/D converter a
UC   A2DSetGain(int a, int b);	//Set gains for individual A/D channels
double A2DSetNorm(int a, A2D_SET *b, double c);	//Set A/D nomalization const.
void A2DSetMaster(US a);	//Assign one of the A/D's as timing master
int  A2DInit(int a, US *b);	//Initialize/load A/D converters
void A2DSetCtr(A2D_SET *a);	//Reset the individual channel sample counters
US   A2DReadInts(void);		//Read the state of all 8 A/D interruptlines
US   A2DSetVcal(int Vx8);	//Set the calibration point voltage
void A2DSetCal(A2D_SET *a);	//Set the cal enable bits for the 8 channels
void A2DSetOffset (A2D_SET *a);	//Set the offset enable bits for the 8 channels
void A2DReadFIFO(int a, US *b);	//Read a shorts from the FIFO into buffer b
void A2DReadDirect(int a,int b,US *c);	//Read b values dir to buf c from A/D a
void A2DSetSYNC(void);		//Set the SYNC bit high-stops A/D conversion
void A2DClearSYNC(void);	//Clear the SYNC bit
void A2D1PPSEnable(void);	//Enables A/D start on next 1 PPS GPS transition
void A2D1PPSDisable(void);	//Disable 1PPS sync
void A2DClearFIFO(void);	//Clear (reset) the FIFO
void A2DError(int a);		//A/D Card error handling

int  init_module(void);
void cleanup_module(void);
/*
static int __init a2d_init();		//For Linux kernel
static void __exit a2d_cleanup(void);	//For Linux kernel
*/

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define A2D_MAGIC 'A'

#define	RTL_DEBUGIT(a)	rtl_printf("DEBUGIT %d\n", a)
#define	DEBUGIT(a)	printf("DEBUGIT %d\n", a)

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define A2D_GET_IOCTL _IOR(A2D_MAGIC,0,A2D_GET)
#define A2D_SET_IOCTL _IOW(A2D_MAGIC,1,A2D_SET)
#define A2D_CAL_IOCTL _IOW(A2D_MAGIC,2,A2D_SET)
#define A2D_RUN_IOCTL _IOW(A2D_MAGIC,3,int)

// Now in source code #include <ioctl_fifo.h>

#ifdef __KERNEL__

#endif

#endif
