/* Mensor.cc
 
   Mensor digital Pressure  class.
 
   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research
 
   Revisions:
 
*/

#include <Mensor.h>

/******************************************************************************
** Public Functions
******************************************************************************/

Mensor::Mensor ()
{ 
// Constructor. 

  ptog = 0;
  gtog = 0;
  idx = 0;

}

/*****************************************************************************/
const char *Mensor::buffer()
{
  return((const char*)&mensor_blk[gtog]);
}
/*****************************************************************************/
void Mensor::parser(int len)
{
  if (idx > 24)
    return;
  sscanf (&buf[2], "%f",mensor_blk[ptog].press[idx++]);
}
/*****************************************************************************/
void Mensor::secondAlign()

// This routine is to be called at each 1 second clock tick. The Mensor_blk
// buffers are toggled.
{
  gtog = ptog;
  ptog = 1 - ptog;
  idx = 0;
}

