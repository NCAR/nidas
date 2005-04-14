/* Climet.h

   Climet  class.
 
   Original Author: Mike Spowart
   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:
 
*/

#ifndef CLIMET_H
#define CLIMET_H

/* #include <logLib.h> */

#include <dsmctl.h>
#include <header.h>
#include <messageDefs.h>

const int CLIMET_DATA_LEN = 	 2000;
 
// Port configuration.
const int CLIMET_BAUD =             9600;           // baud rate
const int CLIMET_BITS =             8;              // data bits
const int CLIMET_PARITY =           SER_NONE;       // parity
const float CLIMET_STOP =           2.0;            // stop bits
const int CLIMET_FLOW =             FALSE;          // no flow control
 
class Climet {

public:
  Climet (const char *base, const int port, void (*statMsg)(char* msg));
  void secondAlign();           // performs 1 sec data alignment
  void checkStatus();		// check serial port status
  void isr();			// isr for this class
  void initClimet();
  void reqstData();
  const char* buffer() {return (char*)&climet_blk[gtog];}

private:
 
  int   ptog;                                   // put toggle index
  int   gtog;                                   // get toggle index 
  int   idx;					// scan data array index
  int   flag;
  Climet_blk climet_blk[TOG];			// local data block
  void (*statusMsg)(char* msg);                 // status msg handler

  char buf[CLIMET_DATA_LEN];			// raw data buffer
  char msg[DSM_MSG_STRING_SIZE];       		// status message string
  char term_str[3];                             // read termintation string
  char start_climet[6];
  char reset_climet[6];

};

#endif
