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
#include <com8.h>

// Class declarations.
static Parsci *parsci[RTL_MAX_SERIAL]; // digital pressure
static Mensor *mensor[RTL_MAX_SERIAL];  // digital pressure

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
void dsmSync()
{
  fd_set readfds, writefds;
  char strdevr0[10], strdevr1[10], strdevw[10], buf[1000];
  int i, j, max_fd = 0, stat, len;
  int mensor_idx = 0;
  int parsci_idx = 0;
  int gtog = 1;
  int ptog = 0;
  int newSecondFifo; // file pointer to 'toggle' indicator

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

  // Initialize the time generator class.
  newSecondFifo = open("/dev/irig", O_RDONLY);
  max_fd = max(max_fd, newSecondFifo);

  // Initialize the serial interfaces classes.
  serialInit ();
  printf("serial initialized.\n");

// Create the 1 second sync record class.
  sync_rec = new SyncRecord(*dsm_hdr,*dsm_config,*sample_table,mensor,parsci);

/* Create objects and open the FIFOs to their modules */

  for (stat = dsm_hdr->firstDesc(); stat; stat = dsm_hdr->nextDesc()) {
    if (dsm_hdr->mensorType()) {
      mensor[mensor_idx] = new Mensor();
      sprintf(strdevr0, "/dev/read_0_mensor_%1d", mensor_idx);
      sprintf(strdevr1, "/dev/read_1_mensor_%1d", mensor_idx);
      sprintf(strdevw, "/dev/write_mensor_%1d", mensor_idx);
      mensor[mensor_idx]->dataFifo[0] = open(strdevr0, O_RDONLY );
      mensor[mensor_idx]->dataFifo[1] = open(strdevr1, O_RDONLY );
      mensor[mensor_idx]->cmndFifo    = open(strdevw,  O_WRONLY );
      mensor_idx+=2;
    }
    if (dsm_hdr->parsciType()) {
      parsci[parsci_idx] = new Parsci();
      sprintf(strdevr0, "/dev/read_0_parsci_%1d", parsci_idx);
      sprintf(strdevr0, "/dev/read_1_parsci_%1d", parsci_idx);
      sprintf(strdevr0, "/dev/write_parsci_%1d", parsci_idx);
      parsci[parsci_idx]->dataFifo[0] = open(strdevr0, O_RDONLY );
      parsci[parsci_idx]->dataFifo[1] = open(strdevr1, O_RDONLY );
      parsci[parsci_idx]->cmndFifo    = open(strdevw,  O_WRONLY );
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
    for (i=0; i<RTL_MAX_SERIAL; i++) {
      FD_SET(mensor[i]->dataFifo[0],  &readfds);
      FD_SET(mensor[i]->dataFifo[1],  &readfds);
      FD_SET(mensor[i]->cmndFifo   ,  &writefds);

      FD_SET(parsci[i]->dataFifo[0], &readfds);
      FD_SET(parsci[i]->dataFifo[1], &readfds);
      FD_SET(parsci[i]->cmndFifo   , &writefds);
    }
    FD_SET(newSecondFifo, &readfds);

    /* The select command waits for inbound FIFO data to
     * become available for processing.
     */
    select(FD_SETSIZE, &read_mask, NULL, NULL, NULL);

  /* Scan for any data from the instruments to be read. */
    for (i=0; i<RTL_MAX_SERIAL; i++) {
      len = 0;
      if (FD_ISSET( mensor[i]->dataFifo[ptog], &readfds)){
        while (FD_ISSET( mensor[i]->dataFifo[ptog], &readfds)){
          len += read(mensor[i]->dataFifo[ptog], &mensor[i]->buf[0],
                      sizeof(buf));
        }
        mensor[i]->parser(len);
      }
      len = 0;
      if (FD_ISSET( parsci[i]->dataFifo[ptog], &readfds)){
        while (FD_ISSET( parsci[i]->dataFifo[ptog], &readfds)){
          len += read(parsci[i]->dataFifo[ptog], &parsci[i]->buf[0],
                      sizeof(buf));
        }
        parsci[i]->parser(len);
      }
    }

  /* a new second occured... */
    if (FD_ISSET(newSecondFifo, &read_mask))
    {
      /* determine which toggle buffer to read from */
      j = read (newSecondFifo, &ptog, sizeof(int));
      printf("(%d) now reading the periodic /dev/read_???_%d FIFOs\n", j, ptog);

      for (i=0; i<RTL_MAX_SERIAL; i++) {
        mensor[i]->secondAlign();
        parsci[i]->secondAlign();
     }
      // Build the 1 second sync record.
      sync_rec->buildRecord ();

  /* Scan for any data from the instruments to be read. */
      for (i=0; i<RTL_MAX_SERIAL; i++) {
        len = 0;
        if (FD_ISSET( mensor[i]->dataFifo[gtog], &readfds)){
          while (FD_ISSET( mensor[i]->dataFifo[gtog], &readfds)){
            len += read(mensor[i]->dataFifo[gtog], &mensor[i]->buf[0],
                        sizeof(buf));
          }
          mensor[i]->parser(len);
        }
        len = 0;
        if (FD_ISSET( parsci[i]->dataFifo[gtog], &readfds)){
          while (FD_ISSET( parsci[i]->dataFifo[gtog], &readfds)){
            len += read(parsci[i]->dataFifo[gtog], &parsci[i]->buf[0],
                        sizeof(buf));
          }
          parsci[i]->parser(len);
        }
      }

      /* TODO - Start a non-blocking TCP transmission to the ADS. */
      gtog = ptog;
      ptog = 1 - ptog;

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
