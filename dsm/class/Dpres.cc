/* Dpres.cc
 
   Digital Pressure  class.
 
   Original Author: Mike Spowart
   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:
 
*/

#include <Dpres.h>
#include <stdlib.h>

/******************************************************************************
** Public Functions
******************************************************************************/

Dpres::Dpres (const char *base, const int port, 
                void (*statMsg)(char* msg)) : statusMsg (statMsg)

// Constructor. The interrupt routine must be previously set up outside this 
// class.
{

  ptog = 0;
  gtog = 0;
  idx = 0;

  term_str[0] = '\r';
  term_str[1] = '\n';
  term_str[2] = '\0';

  start_press[0] = '$';
  start_press[1] = 'M';
// 'R' requests a single sample, 'C' specifies continuous transmission
  start_press[2] = 'R';
//  start_press[2] = 'C';
  start_press[3] = '\r';

//  pputcnt ((char*)start_press, 4);	// command continuous pressure
//  clrbuf();
//  igetstr (term_str);			// issue first read

}

/*****************************************************************************/
void Dpres::checkStatus()

// Check the serial port status.
{
//   if (checkPort() != TRUE) {
// //    igetstr (term_str);         // new read if an error or dropped interrupt
//     statusMsg ("Dpres: Dropped isio interrupt.\n");
//   } 
}

/*****************************************************************************/
void Dpres::secondAlign()
 
// This routine is to be called at each 1 second clock tick. The Dpres_blk
// buffers are toggled.
{
//  if (idx != 5)
//    logMsg("Dpres: idx == %d\n", idx,0,0,0,0,0);

  gtog = ptog;
  ptog = 1 - ptog;
  idx = 0;

}

/*****************************************************************************/
void Dpres::reqstData ()
 
// This routine is to be called 5 times per second .
{
//   pputcnt((char*)start_press, 4);	// command one pressure
//   igetstr(term_str); 
}

/*****************************************************************************/
void Dpres::isr()
 
// Pulls in data from the isio and builds the Dpres data struct.
{
//   char *datap;

// // Check for data from the isio port.
//   if (!(int)(datap = getData()) || idx > 4)
//     return;
 
// // Check the data alignment.
//   if ((datap[DPRES_SYNC_IDX] == DPRES_SYNC))
//     dpres_blk[ptog].press[idx++] = atof(datap);

}

/*****************************************************************************/
const char *Dpres::buffer()
{

// Make sure no 0.0's leave here.
  for (int i = 0; i < 5; ++i)
    if (dpres_blk[gtog].press[i] == 0.0)
      if (i == 0)
        dpres_blk[gtog].press[i] = prevSample;
      else
        dpres_blk[gtog].press[i] = dpres_blk[gtog].press[i-1];

  prevSample = dpres_blk[gtog].press[4];

  return((const char*)&dpres_blk[gtog]);

}
