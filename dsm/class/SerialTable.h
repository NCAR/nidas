/* SerialTable.h
   Class for serial interface sample tables.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#ifndef SERIALTABLE_H
#define SERIALTABLE_H

#include <TapeHeader.h>

#include <dsmctl.h>
#include <header.h>

class SerialTable {
public:
  SerialTable();			// constructor
  void enter(TapeHeader&, int port);	// make a table entry
  int nextEntry();			// select the next entry
  void displayTable();			// display the table contents
  int firstEntry() 	{ return ((ser_idx = 0) < ser_count); }
  int port()		{ return ser_port[ser_idx]; }
  int start() 		{ return ser_start[ser_idx]; }
  int length() 		{ return ser_length[ser_idx]; }
  char* name() 		{ return ser_name[ser_idx]; }
  char* real_name()	{ return ser_real_name[ser_idx]; }
 
private:
  int ser_port[MAX_SER_INTFC];		// isio port number
  int ser_start[MAX_SER_INTFC];		// block starting offsets
  int ser_length[MAX_SER_INTFC];	// block lengths
  str12 ser_name[MAX_SER_INTFC];	// device names
  str12 ser_real_name[MAX_SER_INTFC];	// device real names
 
  int ser_idx;				// current entry index
  int ser_count;			// number of entries

};

#endif
