/* Neph.cc
 
   Neph  class.
 
   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research
 
   Revisions:
 
*/

#include <Neph.h>
#include <string.h>

/******************************************************************************
** Public Functions
******************************************************************************/

Neph::Neph (const char *base, const int port, 
                void (*statMsg)(char* msg)) : statusMsg (statMsg)

// Constructor. The interrupt routine must be previously set up outside this 
// class.
{
  ptog = 0;
  gtog = 0;
  index = 0;
  memset (buf, 0, sizeof (buf));
  term_str[0] = '\r';
  term_str[1] = '\n';
  term_str[2] = '\0';
}
/*****************************************************************************/
void Neph::initNeph()
{
//   igetstr (term_str);			// issue first read
}
/*****************************************************************************/

void Neph::checkStatus()

// Check the serial port status.
{
//   if (checkPort() != TRUE) {
//     igetstr (term_str);         // new read if an error or dropped interrupt
//     statusMsg ("Neph: Dropped isio interrupt.\n");
//   } 
}
/*****************************************************************************/
void Neph::secondAlign()
 
// Called at the start of a second to align the indices and toggle buffers.
{
  if (index > 0){
//    printf("Index = %d\n",index);

    neph_blk[ptog].bscat = neph_blk[ptog].bscat/index;
    neph_blk[ptog].press = neph_blk[ptog].press/index;
    neph_blk[ptog].temp = neph_blk[ptog].temp/index;
    neph_blk[ptog].RH = neph_blk[ptog].RH/index;
/*
    logMsg("bscat = %d, press = %d, temp = %d, RH = %d\n",
      (int)neph_blk[ptog].bscat, (int)neph_blk[ptog].press,
      (int)neph_blk[ptog].temp,(int)neph_blk[ptog].RH,0,0);
*/
  }

  index = 0;
  gtog = ptog;                          // toggle the get buffer index
  ptog = 1 - gtog;

 
}
/*****************************************************************************/

void Neph::isr()
 
// Pulls in data from the isio and builds the Nephelometer data struct.
{
//   char *datap;
//   float hk1,hk2, hk3,hk4,hk5,hk6;

// //  logMsg("In Neph ISR\n",0,0,0,0,0,0);
// // Check for data from the isio port.
//   if (!(int)(datap = getData()))
//     return;
 
//   logMsg("In Neph ISR\n",0,0,0,0,0,0);
// // Pull in the data.
// //  memcpy(buf, datap, strlen(datap));
//   strcpy(buf, datap);
 
// // Check the data packet type.


//   index +=1;
// // Parse the Neph data string and build the recorded data block.
//   sscanf (&buf[0], "%f %f %f %f %f %f", &hk1, &hk2, &hk3, &hk4, &hk5, &hk6);
//   neph_blk[ptog].cal_coef = hk2;
//   neph_blk[ptog].relay = hk6;
//   if (index == 1){
//     neph_blk[ptog].bscat = hk1;
//     neph_blk[ptog].press = hk3;
//     neph_blk[ptog].temp = hk4;
//     neph_blk[ptog].RH = hk5;
//   }
//   else {
//     neph_blk[ptog].bscat +=hk1;
//     neph_blk[ptog].press +=hk3;
//     neph_blk[ptog].temp +=hk4;
//     neph_blk[ptog].RH +=hk5;
//   }
// /*
//   logMsg("bscat = %d, cal_coef = %d, press = %d, temp = %d,
//           RH = %d, index = %d\n",(int)hk1,(int)hk2, (int)hk3,
//           (int)hk4,(int)hk5,index);
// */
//   igetstr (term_str);                        // issue next read

}
