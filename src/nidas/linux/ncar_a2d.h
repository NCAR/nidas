/* ncar_a2d.h

Header for test rtl driver.

$LastChangedRevision: 3648 $
$LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $
$LastChangedBy: cjw $
$HeadURL: http://svn.atd.ucar.edu/svn/nids/trunk/src/nidas/rtlinux/ncar_a2d.h $

Copyright 2005 UCAR, NCAR, All Rights Reserved

*/

/* 
 * This header is also included from user-side code that wants to get the
 * values of the ioctl commands, and the definition of the structures.
 */

#ifndef NCAR_A2D_H
#define NCAR_A2D_H

#include <nidas/core/dsm_sample.h>              // get dsm_sample typedefs

/* 
 * User programs need these for the _IO macros, but kernel modules get
 * theirs elsewhere.
 */
#ifndef __KERNEL__
#  include <sys/ioctl.h>
#  include <sys/types.h>
#endif


#define MAXA2DS         8       // Max A/D's per card
#define A2D_MAX_RATE    5000
#define INTERRUPT_RATE  100
#define RATERATIO       (A2D_MAX_RATE / INTERRUPT_RATE)


// A/D Filter configuration file parameters
#define CONFBLOCKS      12  // 12 blocks as described below
#define CONFBLLEN       43  // 42 data words plus 1 CRCC (Confirmed by ADI)

/* A2D status info */
typedef struct
{
    // fifoLevel indexes 0-5 correspond to return value of getA2DFIFOLevel
    // 0(empty), 1(<=1/4), 2(<2/4), 3(<3/4), 4(<4/4), 5(full)
    size_t preFifoLevel[6];   // counters for fifo level, pre-read
    size_t postFifoLevel[6];  // counters for fifo level, post-read
    size_t nbad[MAXA2DS];     // number of bad status words in last 100 scans
    unsigned short badval[MAXA2DS];   // value of last bad status word
    unsigned short goodval[MAXA2DS];  // value of last good status word
    unsigned short    ser_num;        // A/D card serial number
    size_t nbadFifoLevel;     // #times hw fifo not at expected level pre-read
    size_t fifoNotEmpty;      // #times hw fifo not empty post-read
    size_t skippedSamples;    // discarded samples because of slow RTL fifo
    int resets;               // number of board resets since last open
} A2D_STATUS;

typedef struct
{
    int  gain[MAXA2DS];    // Gain settings
    int  gainMul[MAXA2DS]; // Gain Code multiplier
    int  gainDiv[MAXA2DS]; // Gain Code divider
    int  Hz[MAXA2DS];      // Sample rate in Hz. 0 is off.
    int  offset[MAXA2DS];  // Offset flags
    long latencyUsecs;     // buffer latency in micro-seconds
    unsigned short filter[CONFBLOCKS*CONFBLLEN+1]; // Filter data
} A2D_SET;

typedef struct
{
    int  calset[MAXA2DS];  // Calibration flags
    int  vcalx8;           // Calibration voltage:
    // 128=0, 48=-10, 208 = +10, .125 V/bit
} A2D_CAL;


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
#define A2D_GET_STATUS _IOR(A2D_MAGIC, 0, A2D_STATUS)
#define A2D_SET_CONFIG _IOW(A2D_MAGIC, 1, A2D_SET)
#define A2D_SET_CAL    _IOW(A2D_MAGIC, 2, A2D_CAL)
#define A2D_RUN        _IO(A2D_MAGIC, 3)
#define A2D_STOP       _IO(A2D_MAGIC, 4)
#define A2D_OPEN_I2CT  _IOW(A2D_MAGIC, 5, int)
#define A2D_CLOSE_I2CT _IO(A2D_MAGIC, 6)
#define A2D_GET_I2CT   _IOR(A2D_MAGIC, 7, short)

//AD7725 Status register bits
#define A2DINSTBSY      0x8000  //Instruction being performed
#define A2DDATARDY      0x4000  //Data ready to be read (Read cycle)
#define A2DDATAREQ      0x2000  //New data required (Write cycle)
#define A2DIDERR        0x1000  //ID error
#define A2DCRCERR       0x0800  //Data corrupted--CRC error
#define A2DDATAERR      0x0400  //Conversion data invalid
#define A2DINSTREG15    0x0200  //Instr reg bit 15
#define A2DINSTREG13    0x0100  //                              13
#define A2DINSTREG12    0x0080  //                              12
#define A2DINSTREG11    0x0040  //                              11
#define A2DINSTREG06    0x0020  //                              06
#define A2DINSTREG05    0x0010  //                              05
#define A2DINSTREG04    0x0008  //                              04
#define A2DINSTREG01    0x0004  //                              01
#define A2DINSTREG00    0x0002  //                              00
#define A2DCONFIGEND    0x0001  //Configuration End Flag.


/* values in the AD7725 Status Register should look like so when
 * the A2Ds are running.  The instruction is RdCONV=0x8d21.
 *
 * bit name             value (X=varies)
 * 15 InstrBUSY         1
 * 14 DataReady         X  it's a mystery why this isn't always set
 * 13 DataRequest       0
 * 12 ID Error          0
 * 11 CRC Error         0
 * 10 Data Error        X  indicates input voltage out of range
 *  9 Inst bit 15       1
 *  8 Inst bit 13       0
 *  7 Inst bit 12       0
 *  6 Inst bit 11       1
 *  5 Inst bit 06       0
 *  4 Inst bit 05       1
 *  3 Inst bit 04       0
 *  2 Inst bit 01       0
 *  1 Inst bit 00       1
 *  0 CFGEND            X probably indicates a bad chip
 */
#define A2DSTATMASK     0xbbfe  // mask for status bits to check
#define A2DEXPSTATUS    0x8252  // expected value of unmasked bits
#define A2DGAIN_MUL     9       // Multiplies GainCode
#define A2DGAIN_DIV     10      // Divides GainCode

#ifdef __KERNEL__
/********  Start of definitions used by the driver module only **********/

#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#define MAX_A2D_BOARDS          4       // maximum number of A2D boards

#define HWFIFODEPTH             1024	// # of words in card's hardware FIFO

/*
 * Size of the circular buffer of samples waiting for processing by
 * taskReadA2DSamples.  This should be one larger than the max number
 * of samples to be buffered, since one element of the circular buffer
 * is always available for the next write.
 */
#define DSMSAMPLE_CBUF_SIZE	5

#define A2DMASTER	0	// A/D chip designated to produce interrupts
#define A2DIOWIDTH	0x10	// Width of I/O space

// address offset for commands to the card itself
//#define A2DCMDADDR	0xF	// address offset for board commands
#define A2DCMDADDR	0xE	// address offset for board commands

/*
 * 500 samples/sec * 8 channels * 2 bytes = 8000 bytes/sec
 */
#define A2D_BUFFER_SIZE         8192

// I/O channels for the A/D card
// To point IO at a channel, first load
//   the channel enable latch by writing
//   the channel number to A2DBASE+A2DIOLOAD
//   e.g. *(unsigned short *)(A2DBASE+A2DIOLOAD) = A2DIOFIFO;
//   will point the enable latch at the FIFO output.

//FIFO Control Word bit definitions
#define A2DIO_FIFO         0x0     // FIFO data (read), FIFO Control (write)
//#define A2DIO_STAT         0x1     // A/D status (read), command (write)
#define A2DIO_WRCMD        0x1     // write a command
#define A2DIO_WRCOEF       0x2     // write a coefficient to one of the 7725s
#define A2DIO_D2A0         0x3
#define A2DIO_D2A1         0x4
#define A2DIO_D2A2         0x5
//#define A2DIO_SYSCTL       0x6     // A/D INT lines(read), Cal/offset (write)
#define A2DIO_RDINTR       0x6     // read A/D INT lines
#define A2DIO_WRCALOFF     0x6     // write cal/offset
#define A2DIO_RDBOARDSTAT  0x7     // read board status
#define A2DIO_WRMASTER     0x7     // set master A/D
#define A2DIO_RDCHANSTAT   0x9     // read status from a specific A/D channel
#define A2DIO_RDDATA       0xa     // read data

//#define A2DIO_STATRD       0x9     // Same as A2DIO_STAT; BSD3(=A2DRWN) high (rd)
//#define A2DIO_CMNDWR       0x1     // Same as A2DIO_STAT; BSD3(=A2DRWN) low  (wr)
//#define A2DDATARD       0xA     // Same as A2DIO_DATA; BSD3(=A2DRWN) high (rd)    // NOT USED
//#define A2DCONFWR       0x2     // Same as A2DIO_DATA; BSD3(=A2DRWN) low  (wr)

// AD7725 chip command words (See A2DIO_WR7725CMD above)
#define AD7725_READID      0x8802  // Read device ID (NOT USED)
#define AD7725_READDATA    0x8d21  // Read converted data
#define AD7725_WRCONFIG    0x1800  // Write configuration data
#define AD7725_WRCONFEM    0x1A00  // Write configuration, mask data (NOT USED)
#define AD7725_ABORT       0x0000  // Soft reset; still configured
#define AD7725_BFIR        0x2000  // Boot from internal ROM (NOT USED)

// A/D Control bits
#define FIFOCLR        0x01    // Cycle this bit 0-1-0 to clear FIFO
#define A2DAUTO        0x02    // Set = allow A/D's to run automatically
#define A2DSYNC        0x04    // Set then cycle A2DSYNCCK to stop A/D's
#define A2DSYNCCK      0x08    // Cycle to latch A2DSYNC bit value
#define A2D1PPSEBL     0x10    // Set to allow GPS 1PPS to clear SYNC
#define FIFODAFAE      0x20    // Set to clamp value of AFAE in FIFO     // NOT USED
#define A2DSTATEBL     0x40    // Enable A2D status
#define FIFOWREBL      0x80    // Enable writing to FIFO. (not used)     // NOT USED

// FIFO Status bits
#define FIFOHF         0x01    // FIFO half full
#define FIFOAFAE       0x02    // FIFO almost full/almost empty
#define FIFONOTEMPTY   0x04    // FIFO not empty
#define FIFONOTFULL    0x08    // FIFO not full
#define INV1PPS        0x10    // Inverted 1 PPS pulse
#define PRESYNC        0x20    // Presync bit			// NOT USED

typedef struct
{
    dsm_sample_time_t timetag; // timetag of sample
    dsm_sample_length_t length;    // number of bytes in data
    short data[RATERATIO*MAXA2DS];
} A2DSAMPLE;

typedef struct
{
    dsm_sample_time_t timestamp; // timetag of sample
    dsm_sample_length_t size;    // number of bytes in data
    short data;
} I2C_TEMP_SAMPLE;

struct A2DBoard {
    unsigned int base_addr;           // Base address of board
    unsigned int cmd_addr;            // Address for commands to the board

    struct tasklet_struct setupTasklet;
    struct completion setupCompletion;
    int setupStatus;             // non-zero if not set up

    struct tasklet_struct resetTasklet;
    int resetStatus;             // non-zero if not set up
    
    int i2cTempRate;             // rate to query I2C temperature sensor
    struct ioctlHandle* ioctlhandle;
    A2D_SET config;              // board configuration
    A2D_CAL cal;                 // calibration configuration
    A2D_STATUS cur_status;       // status info maintained by driver
    A2D_STATUS prev_status;      // status info maintained by driver
    unsigned char requested[MAXA2DS]; // 1=channel requested, 0=isn't
    int nRequestedChannels;      // # of requested channels
    int MaxHz;                   // Maximum requested A/D sample rate
    int ttMsecAdj;               // how much to adjust sample time-tags backwds
    int busy;
    int interrupted;
    size_t readCtr;
    int nbadScans;
    int expectedFifoLevel;
    int master;
    int sampsPerCallback;     // data values per IRIG callback per channel
    int latencyCnt;           // accumulate this many samples before 
                              // allowing user-space read to continue

    size_t sampleCnt;         // sample counter

    size_t nbadFifoLevel;
    size_t fifoNotEmpty;
    size_t skippedSamples;    // how many samples have we missed?
    int resets;               // number of board resets since last open

    struct kfifo* buffer;     // holds data for transfer to user space
    spinlock_t bufLock;       // lock for data buffer
    wait_queue_head_t rwaitq; // wait queue for user reads

    unsigned short OffCal;    // offset and cal bits
    unsigned char FIFOCtl;    // hardware FIFO control word storage
    short i2cTempData;        // last measured temperature
    unsigned char i2c;        // data byte written to I2C
    char invertCounts;        // whether to invert counts from this A2D
    char doTemp;              // fetch temperature after next A2D scan
    char discardNextScan;     // should we discard the next scan
    int enableReads;
};

#endif

#endif
