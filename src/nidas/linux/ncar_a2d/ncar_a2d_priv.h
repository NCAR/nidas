/* ncar_a2d_priv.h

NCAR A/D driver private header

$LastChangedRevision: 3648 $
$LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $
$LastChangedBy: cjw $
$HeadURL: http://svn.atd.ucar.edu/svn/nids/trunk/src/nidas/rtlinux/ncar_a2d.h $

Copyright 2005 UCAR, NCAR, All Rights Reserved

*/

#ifndef NCAR_A2D_PRIV_H
#define NCAR_A2D_PRIV_H

#include <nidas/linux/ncar_a2d.h>       // shared stuff

#include <linux/wait.h>

#define A2D_MAX_RATE    5000

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

#define MAX_A2D_BOARDS          4       // maximum number of A2D boards

#define HWFIFODEPTH             1024    // # of words in card's hardware FIFO
#define FIFO_SAMPLE_QUEUE_SIZE 128        // must be power of 2
#define A2D_SAMPLE_QUEUE_SIZE 2048       // must be power of 2

#define A2DMASTER	0       // A/D chip designated to produce interrupts
#define A2DIOWIDTH	0x10    // Width of I/O space

/*
 * address offset for commands to the card itself
 *
 * (Note that because of PC/104 16-bit card issues with Vulcan CPUs, 
 * NCAR A/D cards for use on Vulcans must have alternate CPLD logic
 * to use 0xE as the card command address, rather than 0xF.  A side
 * effect of this change limits us to using A/D channels 0-6, i.e.,
 * we have 7 rather than 8 channels available.)
 */
#if defined(CONFIG_MACH_ARCOM_MERCURY) || defined(CONFIG_MACH_ARCOM_VULCAN)
#  define A2DCMDADDR	0xE
#define NUM_USABLE_NCAR_A2D_CHANNELS 7
#else
#  define A2DCMDADDR	0xF
#define NUM_USABLE_NCAR_A2D_CHANNELS NUM_NCAR_A2D_CHANNELS
#endif

// I/O channels for the A/D card
// To point IO at a channel, first load
//   the channel enable latch by writing
//   the channel number to A2DBASE+A2DIOLOAD
//   e.g. *(unsigned short *)(A2DBASE+A2DIOLOAD) = A2DIOFIFO;
//   will point the enable latch at the FIFO output.

// FIFO Control Word bit definitions [BSD(3)(2)(1)(0)>
#define A2DIO_FIFO         0x0  // FIFO data (read), FIFO Control (write)
#define A2DIO_A2DSTAT      0x1  // write a command
#define A2DIO_A2DDATA      0x2  // write a coefficient to one of the 7725s
#define A2DIO_D2A0         0x3  // 
#define A2DIO_D2A1         0x4  // 
#define A2DIO_D2A2         0x5  // 
#define A2DIO_SYSCTL       0x6  // read A/D INT lines, write cal/offset
#define A2DIO_FIFOSTAT     0x7  // read board status,  set master A/D
#define A2DIO_LBSD3        0x8  // add this to A2DSTAT or A2DDATA above to read instead of write

// AD7725 chip command words (See A2DIO_A2DSTAT above)
#define AD7725_READID      0x8802       // Read device ID (NOT USED)
#define AD7725_READDATA    0x8d21       // Read converted data
#define AD7725_WRCONFIG    0x1800       // Write configuration data
#define AD7725_WRCONFEM    0x1A00       // Write configuration, mask data (NOT USED)
#define AD7725_ABORT       0x0000       // Soft reset; still configured
#define AD7725_BFIR        0x2000       // Boot from internal ROM (NOT USED)

// A/D Control bits
#define FIFOCLR        0x01     // [FIFOCTL(0)>  Cycle this bit 0-1-0 to clear FIFO
#define A2DAUTO        0x02     // [FIFOCTL(1)>  Set = allow A/D's to run automatically
#define A2DSYNC        0x04     // [FIFOCTL(2)>  Set then cycle A2DSYNCCK to stop A/D's
#define A2DSYNCCK      0x08     // [FIFOCTL(3)>  Cycle to latch A2DSYNC bit value
#define A2D1PPSEBL     0x10     // [FIFOCTL(4)>  Set to allow GPS 1PPS to clear SYNC
#define FIFODAFAE      0x20     // [FIFOCTL(5)>  Set to clamp value of AFAE in FIFO     // NOT USED
#define A2DSTATEBL     0x40     // [FIFOCTL(6)>  Enable A2D status
#define FIFOWREBL      0x80     // [FIFOCTL(7)>  Enable writing to FIFO. (not used)     // NOT USED

// FIFO Status bits
#define FIFOHF         0x01     // FIFO half full
#define FIFOAFAE       0x02     // FIFO almost full/almost empty
#define FIFONOTEMPTY   0x04     // FIFO not empty
#define FIFONOTFULL    0x08     // FIFO not full
#define INV1PPS        0x10     // Inverted 1 PPS pulse
#define PRESYNC        0x20     // Presync bit                   // NOT USED

struct a2d_sample
{
        dsm_sample_time_t timetag;      // timetag of sample
        dsm_sample_length_t length;     // number of bytes in data
	unsigned short id;
        short data[NUM_NCAR_A2D_CHANNELS];
};

struct A2DBoard
{
        unsigned long base_addr; // Base address of board
        unsigned long cmd_addr;  // Address for commands to the board

        char deviceName[32];

        int gain[NUM_NCAR_A2D_CHANNELS];    // Gain settings
        int offset[NUM_NCAR_A2D_CHANNELS];  // Offset flags
        unsigned short ocfilter[CONFBLOCKS*CONFBLLEN+1]; // on-chip filter data

        int scanRate;           // how fast to scan the channels
        int scanDeltatMsec;     // dT between A2D scans
        int pollRate;           // how fast to poll the FIFO
        int irigRate;           // poll irigClockRate (e.g.: IRIG_100_HZ)
        int nFifoValues;        // How many FIFO values to read every poll
        int skipFactor;         // set to 2 to skip over interleaving status
        int busy;
        int interrupted;
        unsigned int readCtr;
        int nbadScans;
        int master;

        struct dsm_sample_circ_buf fifo_samples;        // samples for bottom half
        struct dsm_sample_circ_buf a2d_samples; // samples out of b.h.
        wait_queue_head_t rwaitq_a2d;   // wait queue for user reads of a2d
        struct sample_read_state a2d_read_state;

        int nfilters;           // how many different output filters
        struct a2d_filter_info *filters;

        struct irig_callback* a2dCallback;

        struct irig_callback* tempCallback;

        int tempRate;           // rate to query I2C temperature sensor
        short currentTemp;

        unsigned int nbadFifoLevel;
        unsigned int fifoNotEmpty;
        unsigned int skippedSamples;  // how many samples have we missed?

        struct work_struct resetWorker;
        volatile int errorState;
        int resets;             // number of board resets since last open

        unsigned short OffCal;  // offset and cal bits
        unsigned char FIFOCtl;  // hardware FIFO control word storage
        unsigned char i2c;      // data byte written to I2C

        char discardNextScan;   // should we discard the next scan

        struct work_struct sampleWorker;

        long latencyMsecs;      // buffer latency in milli-seconds
        long latencyJiffies;    // buffer latency in jiffies
        unsigned long lastWakeup;       // when were read & poll methods last woken

        struct ncar_a2d_cal_config cal;            // calibration configuration
        struct ncar_a2d_status cur_status;  // status info maintained by driver
        struct ncar_a2d_status prev_status; // status info maintained by driver

};
#endif
