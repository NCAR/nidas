/* dsmSync.cc
   DSM sync program to run on an Arcom Viper board.

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
#include <Mensor.h>             // Mensor serial pressure
#include <Parsci.h>             // Parascientific serial pressure

// database class includes
#include <DsmConfig.h>          // sram net config
#include <SampleTable.h>        // sample table class
#include <SyncRecord.h>         // synchronous 1 sec record class
#include <TapeHeader.h>         // sram tapeheader
#include <VarConfig.h>          // derived variable config
#include <dsmctl.h>
#include <header.h>

// module driver includes
#include <main.h>
#include <rtl_com8.h>

// Class declarations.
static Irig   *irig;
static Mensor *mensor[MAX_SERIAL];  // digital pressure
static ParSci *parsci[MAX_SERIAL]; // digital pressure

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

// database class declarations
DsmConfig *dsm_config;                  // sram net config
static SampleTable *sample_table;       // sample table class
static SyncRecord *sync_rec;		// synchronous 1 sec record class
static TapeHeader *dsm_hdr;             // sram tapeheader
static VarConfig *var_config;    	// derived variable config

static void statusMsg (char *msg);      // status message handler
static void serialInit();               // init serial interfaces

/*****************************************************************************/
void main()
{
  fd_set readfds, writefds;
  char strdev[10], tick, buf[1000];
  int i, j, max_fd, stat, len;
  int mensor_idx = 0;
  int parsci_idx = 0;
  int gtog = 0;
  char *host, *adsname;

  printf ("user main: %s - %s - %s\n\n", __FILE__, __DATE__, __TIME__);

/* Create and initialize the tape header class.  */
  dsm_hdr =  new TapeHeader();
  fprintf (stderr, "Header file = %s.\n", dsm_config->dsmHeaderName());
  dsm_hdr->readFile (dsm_config->dsmHeaderName());

/* Create and initialize the derived variable configuration class.  */
/*
  if (!(adsname = getenv ("ADSNAME") ) )
  {
    printf ("dsmSync: ADSNAME environment variable not set.\n");
    exit( ERROR );
  }
  var_config = new VarConfig ( strtok (adsname, "."), dsm_config->location() );
*/
// Create the sample table class.
  sample_table = new SampleTable (*dsm_hdr);
  sample_table->buildTable ();

  printf ("dsmSync: DEBUG everything loaded!\n"); 

  /* TODO - parse XML configuration file into a dsm_config
   * structure.
   */

  /* TODO - convey configuration down to the RTLinux modules
   */

  int i, j, max_fd=0, gtog=0;

  // Initialize the time generator class.
  irig = new Irig();
  irig->newSecondFifo = open("/dev/irig", O_RDONLY);
  max_fd = max(max_fd, irig->newSecondFifo);

  // Initialize the serial interfaces classes.
  serialInit ();
  printf("serial initialized.\n");

// Create the 1 second sync record class.
  sync_rec = new SyncRecord(*dsm_hdr,*dsm_config,*sample_table,mensor,parsci);

/* Create objects and open the FIFOs to their modules */

  for (stat = firstDesc(); stat; stat = nextDesc()) {
    if (mensorType()) {
      mensor[mensor_idx] = new Mensor();
      sprintf(strdev, "mensor_%d", mensor_idx)
      mensor[mensor_idx]->dataFifo[0] = open("/dev/read_0_"+strdev, O_RDONLY );
      mensor[mensor_idx]->dataFifo[1] = open("/dev/read_1_"+strdev, O_RDONLY );
      mensor[mensor_idx]->cmndFifo    = open("/dev/write_"+strdev,  O_WRONLY );
      mensor_idx+=2;
    }
    if (parsciType()) {
      mensor[parsci_idx] = new Parsci();
      sprintf(strdev, "parsci_%d", parsci_idx)
      mensor[parsci_idx]->dataFifo[0] = open("/dev/read_0_"+strdev, O_RDONLY );
      mensor[parsci_idx]->dataFifo[1] = open("/dev/read_1_"+strdev, O_RDONLY );
      mensor[parsci_idx]->cmndFifo    = open("/dev/write_"+strdev,  O_WRONLY );
      parsci_idx+=2;
    }
  }
  max_fd = 1 + parsci_idx + mensor_idx;
  fd_set read_mask;

  /* Main loop for gathering data, sending commands,
   * and transmitting data via network to ADS server.
   */
  while (1)
  {
    /* initialize fd's to zero */
    FD_ZERO(&read_mask);
    FD_ZERO(&writefds);

  /* set the fd's to read data from and write commands to */
  for (i=0; i<MAX_SERIAL; i++) {
    FD_SET(mensor[i]->dataFifo[0],  &readfds);
    FD_SET(mensor[i]->dataFifo[1],  &readfds);
    FD_SET(mensor[i]->cmndFifo   ,  &writefds);

    FD_SET(parsci[i]->dataFifo[0], &readfds);
    FD_SET(parsci[i]->dataFifo[1], &readfds);
    FD_SET(parsci[i]->cmndFifo   , &writefds);
  }
  FD_SET(irig->newSecondFifo, &readfds);

    /* The select command waits for inbound FIFO data to
     * become available for processing.
     */
    select(FD_SETSIZE, &read_mask, NULL, NULL, NULL);

  /* Scan for any data from the instruments to be read. */
  for (i=0; i<MAX_SERIAL; i++) {
    for (j=0; j<2; j++) {
      if (FD_ISSET( mensor[i]->dataFifo[j], &readfds)){
        len = read(mensor[i]->dataFifo[j], &mensor[i]->buf[0], sizeof(buf));
        if (buf[0] == '0')
          mensor[i]->parser();
      }
      if (FD_ISSET( parsci[i]->dataFifo[j], &readfds)){
        len = read(parsci[i]->dataFifo[j], &parsci[i]->buf[0], sizeof(buf));
        if (buf[0] == '*')
          parsci[i]->parser();
      }
    }
  }

    /* a new second occured... */
    if (FD_ISSET(irig->newSecondFifo, &read_mask))
    {
      /* determine which toggle buffer to read from */
      j = read (irig->newSecondFifo, &gtog, sizeof(int));
      printf("(%d) now reading the periodic /dev/read_???_%d FIFOs\n", j, gtog);

      for (i=0; i<MAX_SERIAL; i++) {
        mensor[i]->secondAlign();
        parsci[i]->secondAlign();
     }
      // Build the 1 second sync record.
      sync_rec->buildRecord ();

      /* Parse dataFifos as generic character streams into specific
       * data structs for serially interfaced instruments only.
       */

      /* TODO - Start a non-blocking TCP transmission to the ADS. */
      if (gtog) gtog = 0;
      else      gtog = 1;

    }
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
