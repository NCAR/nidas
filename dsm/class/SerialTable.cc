/* SerialTable.cc
   Class for managing the Honeywell irs interface sample table.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#include <SerialTable.h>

/******************************************************************************
** Public Functions
******************************************************************************/
SerialTable::SerialTable () 
{
  memset((char*)ser_port, 0, sizeof (ser_port));
  memset((char*)ser_start, 0, sizeof (ser_start));
  memset((char*)ser_length, 0, sizeof (ser_length));
  memset((char*)ser_name, 0, sizeof (ser_name));
  memset((char*)ser_real_name, 0, sizeof (ser_real_name));

  ser_idx = 0;
  ser_count = 0;
}

/*****************************************************************************/
void SerialTable::enter (TapeHeader &dsm_hdr, int port)

// Make a table entry. dsm_hdr is passed in with the descriptor selected for
// the channel to be entered.
{
  printf("SerialTable::enter - dsm_hdr.start()     = %d\n", dsm_hdr.start() );
  printf("SerialTable::enter - dsm_hdr.length()    = %d\n", dsm_hdr.length() );
  printf("SerialTable::enter - dsm_hdr.item_type() = %s\n", dsm_hdr.item_type() );
  printf("SerialTable::enter - dsm_hdr.locn()      = %s\n", dsm_hdr.locn() );
  printf("SerialTable::enter - dsm_hdr.name()      = %s\n", dsm_hdr.name() );
  return;

  ser_port[ser_count] = port;
  ser_start[ser_count] = dsm_hdr.start();
  ser_length[ser_count] = dsm_hdr.length();

  strcpy(ser_name[ser_count], dsm_hdr.item_type());
  strcat(ser_name[ser_count], "_");
  strcat(ser_name[ser_count], dsm_hdr.locn());
  strcpy(ser_real_name[ser_count], dsm_hdr.name());

  ser_count++;
}

/*****************************************************************************/
int SerialTable::nextEntry ()
 
// Select the next channel entry.
{
  if ((ser_idx + 1) < ser_count) {
    ser_idx++;
    return TRUE;
  }
  return FALSE;
}

/*****************************************************************************/
void SerialTable::displayTable ()

// Display the table contents.
{
  int j;

  for (j = 0; j < ser_count; j++) 
    printf ("name %s, port %1d, start %4d length %3d\n",
            ser_name[j], ser_port[j], ser_start[j], ser_length[j]);
}
