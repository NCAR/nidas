/* Mensor.h

   Mensor digital pressure instrument class.
 
   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research
 
   Revisions:
 
*/

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
  void parser();
  void secondAlign();
  char buf[1000];
  struct Mensor_blk {
    float press[5];                               /* Digital pressure */
  };
  typedef struct Mensor_blk Mensor_blk;

private:
  Mensor_blk mensor_blk[TOG];			// local data block
  int ptog;
  int gtog;
  int idx;
};
