/* Neph.h

   Neph  class.
 
   Original Author: Mike Spowart
   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:
 
*/

#ifndef NEPH_H
#define NEPH_H

/* #include <logLib.h> */

#include <dsmctl.h>
#include <header.h>
#include <messageDefs.h>

const int NEPH_DATA_LEN = 	 2000;
 
// Port configuration.
const int NEPH_BAUD =             9600;           // baud rate
const int NEPH_BITS =             8;              // data bits
const int NEPH_PARITY =           SER_NONE;       // parity
const float NEPH_STOP =           1.0;            // stop bits
const int NEPH_FLOW =             FALSE;          // no flow control
 
class Neph {

public:
  Neph (const char *base, const int port, void (*statMsg)(char* msg));
  void secondAlign();           // performs 1 sec data alignment
  void checkStatus();		// check serial port status
  void isr();			// isr for this class
  void initNeph();
  const char* buffer() {return (char*)&neph_blk[gtog];}

private:
 
  int   ptog;                                   // put toggle index
  int   gtog;                                   // get toggle index 
  int   index;					// scan data array index
  Neph_blk neph_blk[TOG];			// local data block
  void (*statusMsg)(char* msg);                 // status msg handler

  char buf[NEPH_DATA_LEN];			// raw data buffer
  char msg[DSM_MSG_STRING_SIZE];       		// status message string
  char term_str[3];                             // read termintation string
};

#endif
