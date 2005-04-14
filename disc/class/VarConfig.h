/* VarConfig.h
   Reads derived variable configuration information from the varconfig file.

   Original Author: Jerry V. Pelk
   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/
 
#ifndef VARCONFIG_H
#define VARCONFIG_H
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dsmctl.h>
#include <header.h>

#define VAR_MAX_ENTRIES		10	// max number of variable entries
#define VAR_MAX_DEPEND		4	// max number of dependencies per var
#define VAR_MAX_LINE		80	// max characters per line

class VarConfig {
public:
  VarConfig (const char *host, const char *locn);    	// constructor
  int nextVar();					// select next entry
  int nextDepend();					// select next dep
  int selectByName (const char* name); 			// select by var name

  int firstVar() 	{firstDepend(); return ((var_idx = 0) < var_cnt);}
  int firstDepend() 	{return ((dep_idx = 0) < dep_cnt[var_idx]);}
  const char *varName() {return var_name[var_idx];}
  const char *dependName() 	{return dep_name[var_idx][dep_idx];}
 
private:
  void openConfig (const char *host);		// open the config file
  void readLabelLines();			// read past the label lines
  void readConfigLines (const char *locn);	// read a file entry line

  void closeConfig() 	{fclose (fp);}

  FILE *fp;
  str12 var_name[VAR_MAX_ENTRIES];        		// variable names
  str12 dep_name[VAR_MAX_ENTRIES][VAR_MAX_DEPEND];   	// dependency names
  int dep_cnt[VAR_MAX_ENTRIES];				// number of deps
  int dep_idx;						// current dep index
  int var_cnt;						// number of entries
  int var_idx;						// current var index
};
 
#endif
