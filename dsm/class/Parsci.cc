/* Parsci.cc
 
   Parsci digital Pressure  class.
 
   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research
 
   Revisions:
 
*/

#include <Parsci.h>

/******************************************************************************
** Public Functions
******************************************************************************/

Parsci::Parsci () 
{ 
// Constructor. 

  ptog = 0;
  gtog = 0;
  idx = 0;

}

/*****************************************************************************/
const char *Parsci::buffer()
{
  return((const char*)&parsci_blk[gtog]);
}
/*****************************************************************************/
void Parsci::parser()
{
  if (idx > 4)
    return;
  sscanf (&buf[2], "%f",parsci_blk[ptog].press[idx++]);

}
/*****************************************************************************/
void Parsci::secondAlign()

// This routine is to be called at each 1 second clock tick. The Parsci_blk
// buffers are toggled.
{
  gtog = ptog;
  ptog = 1 - ptog;
  idx = 0;
}

