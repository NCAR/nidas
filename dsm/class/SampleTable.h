/* SampleTable.h
   Class for managing the DSM sample table.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#ifndef SAMPLETABLE_H
#define SAMPLETABLE_H

#define AnalogTable   NULL  //#include <AnalogTable.h>
#define AnaAuxTable   NULL  //#include <AnaAuxTable.h>
#define DigitalTable  NULL  //#include <DigitalTable.h>
#define GreyVmeTable  NULL  //#include <GreyVmeTable.h>
#define GreyVmehTable NULL  //#include <GreyVmehTable.h>
#define HwIrsTable    NULL  //#include <HwIrsTable.h>
#define HwGpsTable    NULL  //#include <HwGpsTable.h>
#define McrTable      NULL  //#include <McrTable.h>
#define Pms1VmeTable  NULL  //#include <Pms1VmeTable.h>
#define PmsVme2dTable NULL  //#include <PmsVme2dTable.h>
#define Pms2dhTable   NULL  //#include <Pms2dhTable.h>
#include <SerialTable.h>
#define SynchroTable  NULL  //#include <SynchroTable.h>
#include <TapeHeader.h>
#define UvHyg         NULL  //#include <UvHyg.h>

#include <dsmctl.h>
#include <header.h>

class SampleTable {
public:
  SampleTable (TapeHeader &dh) : dsm_hdr (dh) {};	// constructor
  void buildTable ();			// build the table
  int startHdr ();			// returns HDR block offset
  int lenHdr ();			// returns HDR block length

/*   AnalogTable ana_table;		// analog input sample table */
/*   AnaAuxTable ana_aux_table;		// analog auxiliary input sample table */
/*   DigitalTable counter_table;		// counter input sample table */
/*   DigitalTable dig_in_table;		// digital input sample table */
/*   SynchroTable synchro_table;		// synchro input sample table */
/*   GreyVmehTable greyh_table;		// vme grey scale housekeeping table */
/*   GreyVmeTable greyvme_table;		// vme grey scale table */
/*   HwIrsTable hwirs_table;		// Honeywell IRS table */
/*   HwGpsTable hwgps_table;		// Honeywell GPS table */
/*   McrTable mcr_table;			// Mcr table */
/*   Pms1VmeTable pms1vme_table;		// vme pms 1d table */
/*   PmsVme2dTable pms2d_table;		// vme pms 2d table */
/*   Pms2dhTable pms2dh_table;		// vme pms 2d housekeeping table */
/*   SerialTable ophir3_table;		// Ophir radiometer table */
/*   SerialTable pps_table;		// Collins PPS GPS table */
/*   SerialTable tans2_table;		// Trimble Tans I and II table */
/*   SerialTable garmin_table;		// Garmin table */
/*   SerialTable cmigits3_table;		// Cmidgets3 table */
/*   SerialTable tans3_table;		// Trimble Tans III table */
/*   SerialTable uvh_table;		// UV hygrometer table */
/*  SerialTable dpres_table;		// DPRES table */
  SerialTable mensor_table;		// Mensor table
  SerialTable parsci_table;		// Parsci table
/*   SerialTable aerosol_table;		// AEROSOL table */
/*   SerialTable masspec_table;		// MASSPEC table */
/*   SerialTable jpltdl_table;		// JPL TDL table */
/*   SerialTable lhtdl_table;		// JPL TDL table */
/*  SerialTable climet_table;		// Climet table */
/*   SerialTable mca_table;		// Mca table */
/*  SerialTable neph_table;		// Neph table */
/*   SerialTable rdma_table;		// Rdma table */
/*   SerialTable spp_table;		// SPP table */

private:
  void enterHdr ();			// enter HDR block table values
  void displayFlightDesc ();		// display flight descriptor info

  TapeHeader &dsm_hdr;			// DSM sample header

  int start_hdr;			// HDR block offset
  int len_hdr;				// HDR block length
};

#endif

