/* Dpres.h

   Digital pressure instruments  class.
 
   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research
 
   Revisions:
 
*/

#ifndef DPRES_H
#define DPRES_H

/* #include <logLib.h> */
#include <string.h>
#include <dsmctl.h>
#include <header.h>
#include <messageDefs.h>

const int DPRES_SYNC_IDX = 0;
const int DPRES_SYNC = '*';
//const int DPRES_SYNC = '#';

// Port configuration.
const int DPRES_BAUD = 9600;	       // baud rate
const int DPRES_BITS = 8;              // data bits
const int DPRES_PARITY = SER_NONE;     // parity
const float DPRES_STOP = 1.0;          // stop bits
const int DPRES_FLOW = FALSE;          // no flow control

class Dpres {

public:
  Dpres (const char *base, const int port, void (*statMsg)(char* msg));

  void isr();			// isr for this class
  void secondAlign();		// performs 1 sec data alignment
  void checkStatus();		// check serial port status
  void reqstData();		// request single pressure

  const char* buffer();

private:
  int	ptog;					// put toggle index
  int	gtog;					// get toggle index 
  Dpres_blk dpres_blk[TOG];			// local data block
  void	(*statusMsg)(char* msg);		// status msg handler
  int	idx;
  float	prevSample;

  char term_str[2];                             // read termintation string
  char start_press[9];				// send pressure cmd.
  char change_baud[15];				

};

#endif
