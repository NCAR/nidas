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
  idx = 0;

}

/*****************************************************************************/
void Mensor::buffer()
{
  if (idx > 4)
    return;
  sscanf (&buf[2], "%f",mensor_blk[ptog].press[idx++]);
}
/*****************************************************************************/
void Mensor::secondAlign()

// This routine is to be called at each 1 second clock tick. The Mensor_blk
// buffers are toggled.
{
  ptog = 1 - ptog;
  idx = 0;
}

