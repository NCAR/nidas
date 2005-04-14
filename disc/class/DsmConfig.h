/* DsmConfig.h
   Reads dsm configuration information from the config file.

   Original Author: Jerry V. Pelk
   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#ifndef DSMCONFIG_H
#define DSMCONFIG_H
 
/* #ifdef VXWORKS */
/* #include <vxWorks.h> */
/* #include <mv162.h> */
/* #include <bootLib.h> */
/* #include <taskLib.h> */
/* extern BOOT_PARAMS sysBootParams; */
/* #endif */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dsmctl.h>

#define DSM_CONFIG_YES_STR	"yes"	 	// set param true string
#define CTL_LOCN_STR		"CTL"		// ctl station location str
#define MCR_LOCN_STR		"MCR"		// ctl station location str
#define WINDS_LOCN_STR		"WINDS"		// winds station location str

#define DSM_MAX_LINE            80      	// max characters per line
 
class DsmConfig {
public:
  DsmConfig();           			// constructor
  int nextDsm();				// select next entry
  int selectByHost (char*);       		// select entry by host name
  int selectByLocn (char*);       		// select entry by dsm location
  int selectByIndex (int index);		// select entry by index

  int firstDsm() {return ((dsm_idx = 0) < dsm_cnt);}	// select first entry
  int count() 		{return dsm_cnt;}		// number of entries
  int index() 		{return dsm_idx;}		// current index
  char* hostName() 	{return host_names[dsm_idx];}	// get dsm host name
  char* location() 	{return locations[dsm_idx];}	// get dsm location
  int port() 		{return dsm_ports[dsm_idx];}  	// get dsm port number
  int timeMaster() 	{return time_master[dsm_idx];}	// time master config
  int localRecord()	{return local_record[dsm_idx];}
  int standalone()	{return stand_alone;}
  int nats()		{return nats_[dsm_idx];}
  char* mainHeaderName(){return main_header_name;}	// get main header name
  char* dsmHeaderName()	{return dsm_header_name[dsm_idx];}// dsm header name
  int isCtlLocation()  	{return !strcmp (location(), CTL_LOCN_STR);}
  int isWindsLocation()	{return !strcmp (location(), WINDS_LOCN_STR);}
  int isDsmLocation()  	{return !isCtlLocation() && !isWindsLocation(); }
  int dsmSerialTod()    {return serial_time[dsm_idx];} //serial output DSM 
 
private:
  int openConfig ();			// open the config file
  void closeConfig () 	{fclose (fp);}
		
  void readHeaderName ();		// read the header name entry line
  void readLabelLines ();		// read past the label lines
  void readConfigLines ();		// read a file entry line

  FILE *fp;
  char main_header_name[80];		// main header file name
  char dsm_header_name[MAX_DSM][80];	// dsm header file names
  str12 host_names[MAX_DSM];            // machine host names
  str12 locations[MAX_DSM];            	// machine locations
  int dsm_ports[MAX_DSM];              	// network port numbers
  int time_master[MAX_DSM];		// time master mode flag 
  int serial_time[MAX_DSM];		// serial time mode flag 
  int local_record[MAX_DSM];		// record locally flag
  int nats_[MAX_DSM];			// telemetry mode flag
  int stand_alone;			// stand alone mode flag
  int dsm_cnt;				// total number of dsms
  int dsm_idx;				// current dsm index
};
 
#endif
