/* ncar_a2d.h

Header for test rtl driver.

$LastChangedRevision: 3648 $
$LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $
$LastChangedBy: cjw $
$HeadURL: http://svn.atd.ucar.edu/svn/nids/trunk/src/nidas/rtlinux/ncar_a2d.h $

Copyright 2005 UCAR, NCAR, All Rights Reserved

Original author: Grant Gray
Revisions:
*/

#ifndef NCAR_A2D_H
#define NCAR_A2D_H

#include <nidas/core/dsm_sample.h>              // get dsm_sample typedefs

#ifndef __KERNEL__
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
#ifndef US
#define US unsigned short
#endif
#ifndef UL
#define UL unsigned long
#endif
#ifndef UC
#define UC unsigned char
#endif
#ifndef SS
#define SS short
#endif
#ifndef SL
#define SL long
#endif
#ifndef SC
#define SC char
#endif

#define MAXA2DS         8       // Max A/D's per card
#define A2D_MAX_RATE    5000
#define INTRP_RATE      100
#define RATERATIO       (A2D_MAX_RATE/INTRP_RATE)


// A/D Filter configuration file parameters
#define CONFBLOCKS      12  // 12 blocks as described below
#define CONFBLLEN       43  // 42 data words plus 1 CRCC (Confirmed by ADI)

/* A2D status info */
typedef struct
{
    // fifoLevel indexes 0-5 correspond to return value of getA2DFIFOLevel
    // 0(empty), 1(<=1/4), 2(<2/4), 3(<3/4), 4(<4/4), 5(full)
    size_t preFifoLevel[6];           // counters for fifo level, pre-read
    size_t postFifoLevel[6];          // counters for fifo level, post-read
    size_t nbad[MAXA2DS];             // number of bad status words in last 100 scans
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
    US   filter[CONFBLOCKS*CONFBLLEN+1]; // Filter data
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
#define A2D_CAL_IOCTL  _IOW(A2D_MAGIC, 2, A2D_CAL)
#define A2D_RUN_IOCTL  _IO(A2D_MAGIC, 3)
#define A2D_STOP_IOCTL _IO(A2D_MAGIC, 4)
#define A2D_OPEN_I2CT  _IOW(A2D_MAGIC, 5, int)
#define A2D_CLOSE_I2CT _IO(A2D_MAGIC, 6)
#define A2D_GET_I2CT   _IOR(A2D_MAGIC, 7, short)

//A2D Status register bits
#define A2DINSTBSY      0x8000  //Instruction being performed
#define A2DDATARDY      0x4000  //Data ready to be read (Read cycle)
#define A2DDATAREQ      0x2000  //New data required (Write cycle)
#define A2DIDERR        0x1000  //Chip ID error
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

/* values in the A2D Status Register should look like so when
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

#include <linux/sem.h>

#define MAX_A2D_BOARDS          4       // maximum number of A2D boards

#define HWFIFODEPTH             1024

//Card base address for ISA bus
#define A2DMASTER      0       // A/D chip designated to produce interrupts
#define A2DIOWIDTH     0x10    // Width of I/O space
#define A2DIOLOAD      0xF     // Load A/D configuration data

/*
 * 500samples/sec * 8 channels * 2 bytes = 8000 bytes/sec
 */
#define A2D_BUFFER_SIZE         8192

// I/O channels for the A/D card
// To point IO at a channel, first load
//   the channel enable latch by writing
//   the channel number to A2DBASE+A2DIOLOAD
//   e.g. *(unsigned short *)(A2DBASE+A2DIOLOAD) = A2DIOFIFO;
//   will point the enable latch at the FIFO output.

//FIFO Control Word bit definitions
#define A2DIO_FIFO      0x0     // FIFO data (read), FIFO Control (write)
#define A2DIO_STAT      0x1     // A/D status (read), command (write)
#define A2DIO_DATA      0x2     // A/D data(read), config(write)                 // NOT USED
#define A2DIO_D2A0      0x3
#define A2DIO_D2A1      0x4
#define A2DIO_D2A2      0x5
#define A2DIO_SYSCTL    0x6     // A/D INT lines(read), Cal/offset (write)
#define A2DIO_FIFOSTAT  0x7     // FIFO stat (read), Set master A/D (write)

#define A2DSTATRD       0x9     // Same as A2DIO_STAT; BSD3(=A2DRWN) high (rd)
#define A2DCMNDWR       0x1     // Same as A2DIO_STAT; BSD3(=A2DRWN) low  (wr)
#define A2DDATARD       0xA     // Same as A2DIO_DATA; BSD3(=A2DRWN) high (rd)    // NOT USED
#define A2DCONFWR       0x2     // Same as A2DIO_DATA; BSD3(=A2DRWN) low  (wr)

//A/D Chip command words (See A2DIOSTAT and A2DCMNDWR above)
#define A2DREADID      0x8802  // Read device ID                         // NOT USED
#define A2DREADDATA    0x8d21  // Read converted data
#define A2DWRCONFIG    0x1800  // Write configuration data
#define A2DWRCONFEM    0x1A00  // Write configuration, mask data         // NOT USED
#define A2DABORT       0x0000  // Soft reset; still configured
#define A2DBFIR        0x2000  // Boot from internal ROM                 // NOT USED

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
#define FIFOHF         0x01    // FIFO half full                         // NOT USED
#define FIFOAFAE       0x02    // FIFO almost full/almost empty          // NOT USED
#define FIFONOTEMPTY   0x04    // FIFO not empty
#define FIFONOTFULL    0x08    // FIFO not full
#define INV1PPS        0x10    // Inverted 1 PPS pulse
#define PRESYNC        0x20    // Presync bit                            // NOT USED

typedef struct
{
    dsm_sample_time_t timestamp; // timetag of sample
    dsm_sample_length_t size;    // number of bytes in data
    SS data[RATERATIO*MAXA2DS];
} A2DSAMPLE;

typedef struct
{
    dsm_sample_time_t timestamp; // timetag of sample
    dsm_sample_length_t size;    // number of bytes in data
    short data;
} I2C_TEMP_SAMPLE;

struct A2DBoard {
    unsigned int addr;           // Base address of board
    unsigned int chan_addr;
//    rtl_pthread_t setup_thread;

//    rtl_pthread_t acq_thread;

//    rtl_pthread_t reset_thread;
    void* reset_thread_stack;

    struct sem acq_sem;           // 100Hz semaphore
    int a2dfd;                   // File descriptor of RTL FIFO for A2D data
    char* a2dFifoName;
    int i2cTempfd;               // File descriptor of RTL FIFO for I2C Temp data
    char* i2cTempFifoName;
    int i2cTempRate;             // rate to query I2C temperature sensor
    struct ioctlHandle* ioctlhandle;
    A2D_SET config;              // board configuration
    A2D_CAL cal;                 // calibration configuration
    A2D_STATUS cur_status;       // status info maintained by driver
    A2D_STATUS prev_status;      // status info maintained by driver
    unsigned char requested[MAXA2DS]; // 1=channel requested, 0=isn't
    int MaxHz;                        // Maximum requested A/D sample rate
    int ttMsecAdj;                    // how much to adjust sample time-tags backwds
    int busy;
    int interrupted;
    size_t readCtr;
    int nbadScans;
    int expectedFifoLevel;
    int master;
    int nreads;               // number of reads to empty fifo
    int head;                 // pointer to head of buffer
    int tail;                 // pointer to tail of buffer
    int latencyCnt;           // number of samples to buffer
    size_t sampleCnt;         // sample counter

    size_t nbadFifoLevel;
    size_t fifoNotEmpty;
    size_t skippedSamples;    // discarded samples because of
    // RTL FIFO sluggishness.
    int resets;               // number of board resets since last open

    unsigned char buffer[A2D_BUFFER_SIZE];    // data buffer
    US OffCal;                // offset and cal bits
    UC FIFOCtl;               // hardware FIFO control word storage
    short i2cTempData;        // last measured temperature
    unsigned char i2c;        // data byte written to I2C
    char invertCounts;        // whether to invert counts from this A2D
    char doTemp;              // fetch temperature after next A2D scan
    char discardNextScan;     // should we discard the next scan
    int readActive;
    int enableReads;
};

#endif

#endif
