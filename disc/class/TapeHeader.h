/* TapeHeader.h

   Class definition for creating, reading and manipulating the ADS tape header.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#ifndef TAPEHEADER_H
#define TAPEHEADER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <portable.h>
#include <header.h>
#include <dsmctl.h>

class TapeHeader {
public:
  TapeHeader ();			// constructor

  int readFile(char* fname);		// read the header from a disk file   
  int writeFile(char* fname);		// write the header to a disk file      
  int firstDesc();			// set pointer to first descriptor
  int firstDesc(char blkName[]);
  int getSDI(char varName[]);
  int getPMS2DType(char varName[]);
  int getAsync(char varName[]);
  int nextDesc();			// set pointer to next descriptor
  int nextDesc(char blkName[]);		// set pointer to next descriptor
  int selectShDesc (char *sh_name);	// select an Sh descriptor
  void enterDesc (char* new_desc, int len); // append descriptor to the header
  void calcDescFields ();		// calculate start and offset fields
  int start();				// get start offset of data block
  int length();				// get length of data block
  void coefficients (float cof[]);      // get coefficients values
  char *name();				// get the name field of the variable
  int rate();				// get sample rate of variable
  int sampleLength();			// get length of 1 sample
  char* dsm_locn();			// get dsm location  
  char* locn();				// get instrument aircraft location  

  char *getHeader()		{return hdr;}
  void setFltnum (char* fltnum)	{strcpy (fl->fltnum, fltnum);}		
  void setDate (int yr, int mo, int dy)	
			{sprintf (fl->date, "%02d/%02d/%04d", mo, dy, 2000+yr);}
  void setTime (int hr, int min, int sec)
			{sprintf (fl->time, "%02d:%02d:%02d", hr, min, sec);}

  void clearDate()	{ fl->date[0] = '\0'; }
  void clearTime()	{ fl->time[0] = '\0'; }

  char*	getDesc()	{ return(desc); }
  char*	item_type()	{ return(desc); }
  int	item_len()	{ return(ntohl(((Sh *)desc)->item_len)); }
  int	offset()	{ return(ntohl(((Sh *)desc)->offset)); }
  float	convert()	{ return(ntohf(((Sh *)desc)->convert)); }
  int	order()		{ return(ntohl(((Sh *)desc)->order)); }
  char	*type()		{ return(((Sh *)desc)->type); }
  int	adsaddr()	{ return(ntohl(((Sh *)desc)->adsaddr)); }
  int	ch_gain()	{ return(ntohl(((Sh *)desc)->ch_gain)); }
  int	ch_offset()	{ return(ntohl(((Sh *)desc)->ch_offset)); }

  int ps_start()	{ return(ntohl(((Pms1v2 *)desc)->ps_start)); }
  int ps_length()	{ return(ntohl(((Pms1v2 *)desc)->ps_length)); }
  int intf_num()	{ return(ntohl(((Pms1v2 *)desc)->intf_num)); }
  int intf_chan()	{ return(ntohl(((Pms1v2 *)desc)->intf_chan)); }

  int spp_intf_chan()	{ return(ntohl(((Pms1v3 *)desc)->intf_chan)); }
  int spp_threshold()	{ return(ntohl(((Pms1v3 *)desc)->threshold)); }
  int spp_transRej()	{ return(ntohl(((Pms1v3 *)desc)->transRej)); }
  int spp_bins()	{ return(ntohl(((Pms1v3 *)desc)->bins)); }
  int spp_dofRej()	{ return(ntohl(((Pms1v3 *)desc)->dofRej)); }
  int spp_avgTransWt()	{ return(ntohl(((Pms1v3 *)desc)->avgTransWt)); }
  int spp_attAccept()	{ return(ntohl(((Pms1v3 *)desc)->attAccept)); }
  int spp_divisorFlag()	{ return(ntohl(((Pms1v3 *)desc)->divisorFlag)); }
  int spp_ct_method()	{ return(ntohl(((Pms1v3 *)desc)->ct_method)); }
  int spp_max_width()	{ return(ntohl(((Pms1v3 *)desc)->max_width)); }
  int spp_range()	{ return(ntohl(((Pms1v3 *)desc)->defaultRange)); }
  void spp_OPCthesholds(unsigned short chans[]);

  int pms2_intf_chan()	{ return(ntohl(((Pms2 *)desc)->intf_chan)); }
  int pms2h_intf_chan()	{ return(ntohl(((Pms2h *)desc)->intf_chan)); }

  char *fltnum()	{ return(fl->fltnum);}
  char *date()		{ return(fl->date);}
  char *projectNumber()	{ return(fl->prnum);}

  int lrlen()		{ return((int)ntohl(fl->lrlen)); }
  int lrppr()		{ return((int)ntohl(fl->lrppr)); }
  int thdrlen()		{ return((int)ntohl(fl->thdrlen)); }

  int analogType()	{return sdiType() && !strcmp(type(),ANALOG_STR);}
  int anaAuxType()	{return sdiType() && !strcmp(type(),ANALOG_AUX_STR);}
  int synchroType()	{return sdiType() && !strcmp(type(),"S");}
  int aerosolType()     {return !strcmp(desc, AEROSOL_STR);}
  int asyType()		{return !strcmp(desc, ASYNC_STR);}
  int blkType()		{return hdrType() || gpsTans2Type() || gpsTans3Type() ||
			  garminType()|| lrncType() || ophir3Type() ||
			  climetType() || hwGpsType() || ppsGpsType() ||
			  masspecType() || jpltdlType() || lhtdlType() ||
			  aerosolType() || dpresType() || rdmaType() || 
			  nephType() || cmigits3Type() || mcaType();}
  int climetType()	{return !strcmp (desc, CLIMET_STR);}
  int mcaType()	        {return !strcmp (desc, MCA_STR);}
  int counterType()	{return sdiType() && 
				 !strncmp (type(), COUNTER_STR, 1);}
  int digitalType()	{return sdiType() && 
				 !strncmp (type(), DIGITAL_STR, 1);}
  int digoutType()	{return sdiType() && 
				 !strcmp (type(),DIGITAL_OUT_STR);}
  int dpresType() 	{return !strcmp (desc, DPRES_STR);} 
  int evtType()		{return !strcmp (desc, EVNT_STR);}
  int flType()		{return !strcmp (desc, TAPEHDR_STR);}
  int gpsTans2Type()	{return !strcmp (desc, GPS_TRIM_STR) || 
                                 !strcmp (desc, GPS_TANS2_STR);}
  int gpsTans3Type()	{return !strcmp (desc, GPS_TANS3_STR);}
  int garminType()	{return !strcmp (desc, GPS_GARMIN_STR);}
  int cmigits3Type()	{return !strcmp (desc, CMIGITS3_STR);}
  int greyhType()	{return !strcmp (desc, GREYVMEH_STR);}
  int hwGpsType()	{return !strcmp (desc, HW_GPS_STR);}
  int hdrType()		{return !strcmp (desc, HDR_STR);}
  int hskpType()	{return !strcmp (desc, HSKP_STR);}
  int insType()		{return !strcmp (desc, INS_STR);}
  int irsType()		{return !strcmp (desc, IRS_STR);}
  int jpltdlType() 	{return !strcmp (desc, H2OTDL_STR);} 
  int lhtdlType() 	{return !strcmp (desc, LHTDL_STR);} 
  int lrncType() 	{return !strcmp (desc, LRNC_STR);} 
  int maspType()        {return !strcmp (desc, MASP_STR);}
  int masspecType()     {return !strcmp (desc, MASSPEC_STR);}
  int mcrType()		{return !strcmp (desc, MCR_STR);}
  int nephType() 	{return !strcmp (desc, NEPH903_STR);} 
  int ophir3Type() 	{return !strcmp (desc, OPHIR3_STR);} 
  int pms1Type()	{return !strcmp (desc, PMS1D_STR);}  
  int pms1v2Type() 	{return !strcmp (desc, PMS1V2_STR);} 
  int rdmaType() 	{return !strcmp (desc, RDMA_STR);} 
  int sppType() 	{return !strcmp (desc, PMS1V3_STR);} 
  int pms2Type()	{return !strcmp (desc, PMS2D_STR);}
  int pms2hType()	{return !strcmp (desc, PMS2DH_STR);}
  int ppsGpsType()	{return !strcmp (desc, PPS_GPS_STR);}
  int sdiType()		{return !strcmp (desc, SDI_STR);}
  int shType()		{return sdiType() || hskpType()|| digoutType();}
  int serialType() 	{return !strcmp (desc, SER_STR);} 
  int uvhygType()	{return !strcmp (desc, UVHYG_STR);}

  void setBlkLocn(char *locn)    { strcpy(((Blk*)desc)->locn, locn); }
  void setShConvert(float cnvrt) { ((Sh*)desc)->convert = htonf(cnvrt); }
  void setShConvertOffset(long cnvrt_off) 
			{ ((Sh*)desc)->convert_offset = htonl(cnvrt_off); }
  long getShConvertOffset() { return(ntohl(((Sh*)desc)->convert_offset)); }
  int pms2Resolution()      { return(ntohs(((Pms2*)desc)->resolution)); }
  char *pms2SerialNumber()  { return(((Pms2*)desc)->serial_num); }

  void setUvhygLocn (char *locn)    {strcpy (((Uvhyg*)desc)->locn, locn);}

private:
  Fl *fl;				// flight descriptor pointer
  char hdr[MX_PHYS];				// header pointer
  char *desc;				// descriptor pointer
  unsigned int desc_cnt;		// descriptor counter
  int sdi_rate[MAX_SDI_RATES];		// sdi sample rates used
  int sdi_rate_cnt[MAX_SDI_RATES];	// number of sdi descs at each rate
  int sdi_rate_idx;			// current sdi_rate array index
};
#endif
