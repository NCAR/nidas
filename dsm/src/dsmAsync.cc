/* dsmAsync.cc
   DSM async program to run on an Arcom Viper board.

   Original Author: Jerry V. Pelk
   Ported by      : John Wasinger

   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

// Linux include files.
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// instrument class includes
#include <Climet.h>             // Climet class
#include <Dpres.h>              // digital pressure
#include <Neph.h>               // Neph interface classes

// database class declarations
#include <DsmConfig.h>          // sram net config
#include <SampleTable.h>        // sample table class
#include <SyncRecord.h>         // synchronous 1 sec record class
#include <TapeHeader.h>         // sram tapeheader
#include <VarConfig.h>          // derived variable config


// -- TEST CLASSES -----------------------------------------------
class Irig_cls
{
public:
  int newSecondFifo; // file pointer to 'toggle' indicator
};

// Class declarations.
static Irig_cls   *irig;
// -- TEST CLASSES -----------------------------------------------


#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

// instrument class declarations
static Climet *climet;                  // Climet class
static Dpres *dpres[MAX_UVHYG_INTFC];   // digital pressure
static Neph *neph[MAX_UVHYG_INTFC];     // Neph interface classes

// database class declarations
DsmConfig *dsm_config;                  // sram net config
static SampleTable *sample_table;       // sample table class
static SyncRecord *sync_rec;		// synchronous 1 sec record class
static TapeHeader *dsm_hdr;             // sram tapeheader
static VarConfig *var_config;    	// derived variable config

#define tfp         NULL  // placeholder (REMOVE)
#define counter     NULL  // placeholder (REMOVE)
#define digital_in  NULL  // placeholder (REMOVE)
// #define dpres       NULL  // placeholder (REMOVE)
#define dsp         NULL  // placeholder (REMOVE)
#define hw_irs      NULL  // placeholder (REMOVE)
#define hw_gps      NULL  // placeholder (REMOVE)
#define mcr         NULL  // placeholder (REMOVE)
#define ophir3      NULL  // placeholder (REMOVE)
#define pms1v       NULL  // placeholder (REMOVE)
#define pms2dh      NULL  // placeholder (REMOVE)
#define tans2       NULL  // placeholder (REMOVE)
#define tans3       NULL  // placeholder (REMOVE)
#define pps         NULL  // placeholder (REMOVE)
#define spp         NULL  // placeholder (REMOVE)
#define uvh         NULL  // placeholder (REMOVE)
#define vm3118      NULL  // placeholder (REMOVE)
#define synchro     NULL  // placeholder (REMOVE)
#define greyh       NULL  // placeholder (REMOVE)
#define jpltdl      NULL  // placeholder (REMOVE)
#define lhtdl       NULL  // placeholder (REMOVE)
#define rdma        NULL  // placeholder (REMOVE)
// #define neph        NULL  // placeholder (REMOVE)
// #define climet      NULL  // placeholder (REMOVE)

static void statusMsg (char *msg);      // status message handler
static void serialInit();		// init serial interfaces
static void tfpInit();			// init time-freq processor

/*****************************************************************************/
int main( void )
{
  printf ("user main: %s - %s - %s\n\n", __FILE__, __DATE__, __TIME__);

  char *host, *adsname;

  // Initialize the sram network configuration class.
//  dsm_config = (DsmConfig*)SRAM_DSMCONFIG_BASE; 
  dsm_config = new DsmConfig();
  if (!(host = getenv ("HOST") ) )
  {
    printf ("dsmAsync: HOST environment variable not set.\n");
    exit( ERROR );
  }
  if (!dsm_config->selectByHost( strtok (host, ".") ) )
  {
    printf ("dsmAsync exiting.\n");
    exit (ERROR);
  }
  /* Create and initialize the tape header class in SRAM.  If
   * not operating in the standalone mode, attempt to read the header file
   * by running the constructor.  Otherwise just use the existing header
   * in SRAM.
   */
//  dsm_hdr = (TapeHeader*)SRAM_TAPEHEADER_BASE;
  if (!dsm_config->standalone())
  {
    dsm_hdr =  new TapeHeader();
    fprintf (stderr, "Header file = %s.\n", dsm_config->dsmHeaderName());
    dsm_hdr->readFile (dsm_config->dsmHeaderName());
  }
  /* Create and initialize the derived variable configuration class in SRAM.  If
     not operating in the standalone mode, attempt to read the varconfig file
     by running the constructor.  Otherwise just use the existing configuration
     in SRAM.
  */
//  var_config = (VarConfig*)SRAM_VARCONFIG_BASE;
  if (!dsm_config->standalone())
  {
    if (!(adsname = getenv ("ADSNAME") ) )
    {
      printf ("dsmAsync: ADSNAME environment variable not set.\n");
      exit( ERROR );
    }
    var_config = new VarConfig ( strtok (adsname, "."), dsm_config->location() );
  }
  // Create the sample table class.
  sample_table = new SampleTable (*dsm_hdr);
  sample_table->buildTable ();

  printf ("dsmAsync: DEBUG everything loaded!\n"); // <------------------------------
  return 0;

  /* TODO - parse XML configuration file into a dsm_config
   * structure.
   */

  /* TODO - convey configuration down to the RTLinux modules
   */

  int i, j, max_fd=0, gtog=0;

  // Initialize the time generator class.
  irig = new Irig_cls();
  irig->newSecondFifo = open("/dev/irig", O_RDONLY);
  max_fd = max(max_fd, irig->newSecondFifo);
  printf("max_fd = %d\n", max_fd);
  tfpInit ();
  printf("tfp initialized.\n");

  // Initialize the serial interfaces classes.
  serialInit ();
  printf("serial initialized.\n");

  // Create the 1 second sync record class.
//sync_rec = new SyncRecord(*dsm_hdr, *dsm_config, *sample_table, *tfp, counter,
//                          digital_in, dpres, dsp, hw_irs, hw_gps, mcr,
//                          ophir3, pms1v, pms2dh, tans2, tans3, garmin, pps, spp, 
//                          uvh, vm3118, synchro, greyh, jpltdl, lhtdl, rdma, neph, 
//                          cmigits3,climet, mca);
  sync_rec = new SyncRecord(*dsm_hdr, *dsm_config, *sample_table,
			                dpres,
			    
			                                                      neph, 
			             climet     );


  /* Create objects and open the FIFOs to their modules */
//   for (int i=0; i<MAX_UVHYG_INTFC; i++)
//   {
//     dpres[i]  = new Dpres();

//     for (j=0; j<2; j++)
//     {
//       /* Note that the periodic dataFifos are not watched by
//        * the 'select' command.  Their buffers are read once
//        * a second when 'select' sees a value on /dev/irig.
//        */
//       sprintf(devstr, "/dev/dpres_%d_read_%d", i, j);
//       dpres[i]->dataFifo[j] = open(devstr, O_RDONLY);

//       /* TODO - open the asyncronous dataFifos here. */
//     }
//   }

  /* Note: fd_set is a 1024 bit mask. */
  fd_set read_mask;

  /* Main loop for gathering data, sending commands,
   * and transmitting data via network to ADS server.
   */
  while (1)
  {
    /* initialize fd's to zero */
    FD_ZERO(&read_mask);

    /* set the fd's to read data from */
    /* TODO - add the asyncronous dataFifo's file ID's here. */
    FD_SET(irig->newSecondFifo, &read_mask);

//     // flag the serial devices to be read from...
//     FD_SET(asdf, &read_mask);

    /* The select command waits for inbound FIFO data to
     * become available for processing.
     */
    select(FD_SETSIZE, &read_mask, NULL, NULL, NULL);

    /* a new second occured... */
    if (FD_ISSET(irig->newSecondFifo, &read_mask))
    {
      /* determine which toggle buffer to read from */
      j = read (irig->newSecondFifo, &gtog, sizeof(int));
      printf("(%d) now reading the periodic /dev/read_???_%d FIFOs\n", j, gtog);

      /* Scan for any data from the instruments to be read. */
      /* TODO - place data into buffer based upon a configured
       * location.
       */

      // Build the 1 second sync record.
      sync_rec->buildRecord ();

      /* Parse dataFifos as generic character streams into specific
       * data structs for serially interfaced instruments only.
       */
//       char buffer[10000];
//       char *sep = " \\t\\n";
//       char *tok, *cb;
//       for (i=0; i<MAX_UVHYG_INTFC; i++)
//       {
//         buffer[ read (dpres[i]->dataFifo[gtog], &buffer, 10000) ] = 0;
// //      printf("'%s'\n", buffer);

//         for (j=0, tok = strtok_r(buffer, sep, &cb); j<5;
//              j++, tok = strtok_r(NULL,   sep, &cb))
//           dpres[i]->dpres_blk[gtog].press[j] = atof(tok);
//         printf("last tok = '%s'\n", tok); // DEBUG - if this is not NULL then we missed something
//       }

      /* TODO - Start a non-blocking TCP transmission to the ADS. */

      /* TEST - just printing here to demonstrate data reception */
//       for (i=0; i<MAX_UVHYG_INTFC; i++)
//       {
//         printf("\ndpres[%d] =\n", i);
//         for (j=0; j<5; j++)
//           printf(" %5.3f", dpres[i]->dpres_blk[gtog].press[j]);
//       }
    }
    printf("\n\n");
  }
}

/*****************************************************************************/
static void serialInit ()

// Initialize the serial interfaces.
{
//   int stat;
//   int idx;
//   char devstr[30];

//   // Create the Climet class object.
//   climet = (Climet*)0;
//   if (sample_table->climet_table.firstEntry())
//   {
//     climet = new Climet ((char*)(A24D16_BASE + ISIO1_BASE), CLIMET_PORT_1,
//                          statusMsg);
//     if (climet == NULL) {
//       printf ("(%s) %s: error creating Climet object", __FILE__, __PRETTYFUNCTION__);
//       exit (ERRNO);
//     }
//     sprintf(devstr, "/dev/isa_serial_%d_read_%d", dev, tog);
//     sprintf(devstr, "/dev/dpres_%d_read_%d", i, j);
//     dpres[i]->dataFifo[j] = open(devstr, O_RDONLY);

//   }
//   // Create the digital pressure class object.
//   for (idx = 0; idx < MAX_UVHYG_INTFC; idx++)
//     dpres[idx] = (Dpres*)0;
//   for (stat = sample_table->dpres_table.firstEntry(), idx = 0;
//        stat; stat = sample_table->dpres_table.nextEntry(), idx++)
//   {
//     dpres[idx] = new Dpres ((char*)(A24D16_BASE + ISIO1_BASE), DPRES_PORT_1+idx,
//                             statusMsg);
//     if (dpres[idx] == NULL) {
//       printf ("(%s) %s: error creating Dpres object", __FILE__, __PRETTYFUNCTION__);
//       exit (ERRNO);
//     }
//   }
//   // Create the Neph class object.
//   for (idx = 0; idx < MAX_UVHYG_INTFC; idx++)
//     neph[idx] = (Neph*)0;
//   for (stat = sample_table->neph_table.firstEntry(), idx = 0;
//        stat; stat = sample_table->neph_table.nextEntry(), idx++)
//   {
//     neph[idx] = new Neph ((char*)(A24D16_BASE + ISIO1_BASE),
//                           NEPH_PORT_1+idx, statusMsg);
//     if (neph[idx] == NULL) {
//       printf ("(%s) %s: error creating Neph object", __FILE__, __PRETTYFUNCTION__);
//       exit (ERRNO);
//     }
//   }
}

/*****************************************************************************/
static void statusMsg (char *msg)
 
// Class wrapper for sending status messages.
{
//   comm_msg->sendStatusMsg (msg);
  fprintf (stderr, msg);
}

/*****************************************************************************/
static void tfpInit ()

// Initialize the time-frequency processor.
{
//   tfp = new Bc635Vme ((char*)(A24D16_BASE + TFP_BASE));

//   if (!dsm_config->timeMaster())
//     tfp->disableJamSync();

//   tfp->setPath();			// set default path packet
//   tfp->selectModulatedInput();          // select modulated irig input
//   tfp->selectModulatedOutput();         // select modulated irig output

// // Sync to the 1PPS if operating as the time master.
//   if (dsm_config->timeMaster())
//     tfp->select1PPSMode();
 
// // Sync to irig time code if operating as a time slave.
//   else
//     tfp->selectTimeCodeMode();
 
// // Set the periodic output at 10 Khz, and a 50 usec pulse width.
//   tfp->setPeriodicOutput(10000, 50);
 
// // Set the major time from the time of day clock.
//   todClock->readTime();

//   if (dsm_config->timeMaster()) {
//     tfp->setMajorTime(todClock->year(), todClock->month(), todClock->date(),
//                   todClock->hour(), todClock->minute(), todClock->second());
//   }

// /*  printf ("tfpInit: TodClock = %02d/%02d/%02d %02d:%02d:%02d\n", 
//           todClock->year(), todClock->month(), todClock->date(), 
//           todClock->hour(), todClock->minute(), todClock->second());
// */
}
