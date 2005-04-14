/* Mensor.h

   Mensor digital pressure instrument class.
 
   Original Author: Mike Spowart
   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:
 
*/

#ifndef MENSOR_H
#define MENSOR_H

#include <string.h>
#include <stdio.h>
#include <dsmctl.h>
#include <header.h>
#include <messageDefs.h>

#define MENSOR_STR       "MENSOR"

class Mensor {

public:
  Mensor (); 
  const char* buffer();
  void parser(int len);
  void secondAlign();
  char buf[1000];
  struct Mensor_blk {
    float press[25];                               /* Digital pressure */
  };
  typedef struct Mensor_blk Mensor_blk;
  int dataFifo[2]; // file pointers to toggled inbound data FIFOs
  int cmndFifo;    // file pointer to outbound command FIFO

private:
  Mensor_blk mensor_blk[TOG];			// local data block
  int ptog;
  int gtog;
  int idx;
};

#endif /*MENSOR_H*/
