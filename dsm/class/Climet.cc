/* Climet.cc
 
   Climet  class.
 
   Original Author: Mike Spowart
   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:
 
*/

#include <Climet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************
** Public Functions
******************************************************************************/

Climet::Climet (const char *base, const int port, 
                void (*statMsg)(char* msg)) : statusMsg (statMsg)

// Constructor. The interrupt routine must be previously set up outside this 
// class.
{
  ptog = 0;
  gtog = 0;
  flag = FALSE;
  memset (buf, 0, sizeof (buf));
  term_str[0] = '\r';
  term_str[1] = '\n';
  term_str[2] = '\0';

  start_climet[0] = 'F';
  start_climet[1] = '\r';
  start_climet[2] = '\n';

  reset_climet[0] = 'T';
  reset_climet[1] = '\r';
  reset_climet[2] = '\n';

//  pputcnt ((char*)reset_climet, 3);    // command continuous pressure
  printf("Climet port = %d\n",port);

}
/*****************************************************************************/
void Climet::initClimet()
{
//   igetstr (term_str);			// issue first read
}
/*****************************************************************************/

void Climet::checkStatus()

// Check the serial port status.
{
//   if (checkPort() != TRUE) {
//     igetstr (term_str);         // new read if an error or dropped interrupt
//     statusMsg ("Climet: Dropped isio interrupt.\n");
//   } 
}
/*****************************************************************************/
void Climet::secondAlign()
 
// Called at the start of a second to align the indices and toggle buffers.
{
  gtog = ptog;                          // toggle the get buffer index
  ptog = 1 - gtog;
//  pputcnt ((char*)reset_climet, 3); 
//  flag = FALSE;
//  idx = 0;
  memset (climet_blk[ptog].counts, 0, sizeof (climet_blk[ptog].counts));

 
}
/*****************************************************************************/
void Climet::reqstData ()

// This routine is to be called 1 times per second .
{
//  pputcnt((char*)start_climet, 3);       // command one pressure
//  logMsg("send Climet start\n",0,0,0,0,0,0);
//   igetstr(term_str);
}

/*****************************************************************************/

void Climet::isr()
 
// Pulls in data from the isio and builds the Climet data struct.
{
//   char *datap;
//   unsigned short counts, pof;

// // Check for data from the isio port.
//   if (!(int)(datap = getData()))
//     return;

// //  if(datap[0] = '>') printf("%s\n",datap); return;
// // Pull in the data.
//   strcpy(buf, datap);
// /*
//   if (strlen(datap) != 22) {
//   logMsg("len = %d datap[0] = %d, datap[1] = %d\n",strlen(datap),
//           datap[0],datap[1],0,0,0); 
// }
// */
//   if(datap[0] == 36 || datap[1] == 36)
//     {
//       flag = TRUE;
//       idx = 0;
//     }
//   else if((datap[7] == 0x25) && (flag == TRUE))  {
//       pof = atoi(&datap[11]);
//       climet_blk[ptog].pof = pof; 
// //      logMsg("pof = %d\n",pof,0,0,0,0,0);
//     }
//   else if((datap[0] == 62) && (flag == TRUE)) 
//     {
// //      sscanf (&buf[7], "%u", &counts);

//       counts = atoi(&datap[7]);
// /*
//    if (counts > 2000) {
//      logMsg("buf[13] = %d, buf[14] = %d, buf[15] = %d, buf[16] = %d
//              buf[17] = %d, buf[18] = %d\n",datap[13],datap[14],
//              datap[15],datap[16], datap[17],datap[18]);
//    }   
// */
// //      logMsg("counts[%d] = %d ptog = %d\n",idx,counts,ptog,0,0,0);

//       if (idx < 6) {
//         climet_blk[ptog].counts[idx] = counts;
//         idx ++;
//       }
//       else flag = FALSE; 
// //      else pputcnt ((char*)reset_climet, 3); 
//     }
//   igetstr (term_str);                        // issue next read
}
