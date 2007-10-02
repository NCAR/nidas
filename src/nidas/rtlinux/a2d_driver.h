/* a2d_driver.h

  Header for test rtl driver.

  $LastChangedRevision$
  $LastChangedDate$
  $LastChangedBy$
  $HeadURL$

  Copyright 2005 UCAR, NCAR, All Rights Reserved

  Original author: Grant Gray
  Revisions:
*/

#ifndef A2D_DRIVER_H
#define A2D_DRIVER_H

#include <nidas/linux/ncar_a2d.h>       // shared stuff
#include <nidas/linux/util.h>

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

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

/********  Start of definitions used by the driver module only **********/

#include <rtl_semaphore.h>
#include <nidas/rtlinux/dsm_viper.h>    // get SYSTEM_ISA_IOPORT_BASE

#define MAX_A2D_BOARDS          4       // maximum number of A2D boards

#define HWFIFODEPTH             1024

/*
 * Output buffer sise.
 * 500samples/sec * 8 channels * 2 bytes = 8000 bytes/sec
 */
#define A2D_OUTPUT_BUFFER_SIZE         8192

#define FIFO_SAMPLE_QUEUE_SIZE 8        // must be power of 2
#define A2D_SAMPLE_QUEUE_SIZE 128       // must be power of 2
#define TEMP_SAMPLE_QUEUE_SIZE 4        // must be power of 2

//Card base address for ISA bus
#define A2DMASTER      0        // A/D chip designated to produce interrupts
#define A2DIOWIDTH     0x10     // Width of I/O space
#define A2DIOLOAD      0xF      // Load A/D configuration data

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
#define A2DIO_SYSCTL    0x6     // A/D INT lines(read),Cal/offset (write)
#define A2DIO_FIFOSTAT  0x7     // FIFO stat (read), Set master A/D (write)

#define A2DSTATRD       0x9     // Same as A2DIO_STAT; BSD3(=A2DRWN) high (rd)
#define A2DCMNDWR       0x1     // Same as A2DIO_STAT; BSD3(=A2DRWN) low  (wr)
#define A2DDATARD       0xA     // Same as A2DIO_DATA; BSD3(=A2DRWN) high (rd)    // NOT USED
#define A2DCONFWR       0x2     // Same as A2DIO_DATA; BSD3(=A2DRWN) low  (wr)

//A/D Chip command words (See A2DIOSTAT and A2DCMNDWR above)
#define A2DREADID      0x8802   // Read device ID                         // NOT USED
#define A2DREADDATA    0x8d21   // Read converted data
#define A2DWRCONFIG    0x1800   // Write configuration data
#define A2DWRCONFEM    0x1A00   // Write configuration, mask data         // NOT USED
#define A2DABORT       0x0000   // Soft reset; still configured
#define A2DBFIR        0x2000   // Boot from internal ROM                 // NOT USED

// A/D Control bits
#define FIFOCLR        0x01     // Cycle this bit 0-1-0 to clear FIFO
#define A2DAUTO        0x02     // Set = allow A/D's to run automatically
#define A2DSYNC        0x04     // Set then cycle A2DSYNCCK to stop A/D's
#define A2DSYNCCK      0x08     // Cycle to latch A2DSYNC bit value
#define A2D1PPSEBL     0x10     // Set to allow GPS 1PPS to clear SYNC
#define FIFODAFAE      0x20     // Set to clamp value of AFAE in FIFO     // NOT USED
#define A2DSTATEBL     0x40     // Enable A2D status
#define FIFOWREBL      0x80     // Enable writing to FIFO. (not used)     // NOT USED

// FIFO Status bits
#define FIFOHF         0x01     // FIFO half full                         // NOT USED
#define FIFOAFAE       0x02     // FIFO almost full/almost empty          // NOT USED
#define FIFONOTEMPTY   0x04     // FIFO not empty
#define FIFONOTFULL    0x08     // FIFO not full
#define INV1PPS        0x10     // Inverted 1 PPS pulse
#define PRESYNC        0x20     // Presync bit    // NOT USED

struct a2d_sample
{
        dsm_sample_time_t timetag;      // timetag of sample
        dsm_sample_length_t length;     // number of bytes in data
        short data[NUM_NCAR_A2D_CHANNELS];
};

struct temp_sample
{
        dsm_sample_time_t timetag;      // timetag of sample
        dsm_sample_length_t length;     // number of bytes in data
        short data;
};

struct A2DBoard
{
        unsigned int addr;      // Base address of board
        unsigned int chan_addr;

        rtl_pthread_t setup_thread;

        rtl_pthread_t reset_thread;
        void *reset_thread_stack;

        rtl_pthread_t bh_thread;        // bottom half thread, does filtering
        void *bh_thread_stack;
        rtl_sem_t bh_sem;       // semaphore to notify bh
        int interrupt_bh;

        int a2dfd;              // File descriptor of RTL FIFO for A2D data
        char *a2dFifoName;
        int tempRate;        // rate to query I2C temperature sensor
        struct ioctlHandle *ioctlhandle;
        A2D_SET config;         // board configuration
        A2D_CAL cal;            // calibration configuration
        A2D_STATUS cur_status;  // status info maintained by driver
        A2D_STATUS prev_status; // status info maintained by driver
        unsigned char requested[NUM_NCAR_A2D_CHANNELS]; // 1=channel requested, 0=isn't
        int busy;
        size_t readCtr;
        int nbadScans;
        int expectedFifoLevel;
        int master;

        struct dsm_sample_circ_buf fifo_samples;        // samples for bottom half
        struct dsm_sample_circ_buf a2d_samples; // samples out of b.h.
        struct dsm_sample_circ_buf temp_samples;        // temperature samples
        struct short_sample*    discardSample;
        short *discardBuffer;   // used for discarding data from the fifo

        unsigned char obuffer[A2D_OUTPUT_BUFFER_SIZE];  // data buffer
        int ohead;              // head of obuffer
        int otail;              // tail of obuffer
        long latencyJiffies;    // minimum interval between output writes
        unsigned long lastWrite;        // last write to output RTL fifo

        int scanRate;
        int scanDeltatMsec;

        int nfilters;           // how many different output filters
        struct a2d_filter_info *filters;

        int nFifoValues;        // how many values to read from FIFO per poll
        int skipFactor;         // set to 2 to skip over interleaving status

        size_t nbadFifoLevel;
        size_t fifoNotEmpty;
        size_t skippedSamples;  // discarded samples because of
        // RTL FIFO sluggishness.
        int resets;             // number of board resets since last open

        unsigned short OffCal;  // offset and cal bits
        unsigned char FIFOCtl;  // hardware FIFO control word storage
        short i2cTempData;      // last measured temperature
        unsigned char i2c;      // data byte written to I2C
        char invertCounts;      // whether to invert counts from this A2D
        char discardNextScan;   // should we discard the next scan
        int enableReads;        // reset not in progress
};
#endif
