/* Parsci.h

   Parascientific digital pressure instrument class.
 
   Original Author: Mike Spowart
   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:
 
*/

#ifndef PARSCI_H
#define PARSCI_H

#include <string.h>
#include <stdio.h>
#include <dsmctl.h>
#include <header.h>
#include <messageDefs.h>

#define PARSCI_STR       "PARSCI"

class Parsci {

public:
  Parsci ();
  const char* buffer();
  void parser(int len);
  void secondAlign();
  char buf[1000];
  struct Parsci_blk {
    float press[25];                               /* Digital pressure */
  };
  typedef struct Parsci_blk Parsci_blk;
  int dataFifo[2]; // file pointers to toggled inbound data FIFOs
  int cmndFifo;    // file pointer to outbound command FIFO

private:
  Parsci_blk parsci_blk[TOG];			// local data block
  int ptog;
  int gtog;
  int idx;
};

#endif /*PARSCI_H*/
