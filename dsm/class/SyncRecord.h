/* SyncRecord.h
   Class for building the 1 second synchronous logical records.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#ifndef SYNCRECORD_H
#define SYNCRECORD_H

#include <stdio.h>
#include <string.h>
#include <header.h>
#include <DsmConfig.h>
#include <dsmctl.h>
#include <com8.h>
#define Climet		NULL //#include <Climet.h>
#define Neph		NULL //#include <Neph.h>
#include <Mensor.h>
#include <Parsci.h>

#define Cmigits3	NULL //#include <Cmigits3.h>
#define Counter		NULL //#include <Counter.h>
#define DigitalIn	NULL //#include <DigitalIn.h>
#define GpsTans3       NULL  //#include <GpsTans3.h>
#define Garmin         NULL  //#include <Garmin.h>
#define HwIrs          NULL  //#include <HwIrs.h>
#define HwGps          NULL  //#include <HwGps.h>
#define Mcr            NULL  //#include <Mcr.h>
#define Mca            NULL  //#include <Mca.h>
#define Ophir3         NULL  //#include <Ophir3.h>
#define Ozone          NULL  //#include <Ozone.h>
#define Pms1Vme        NULL  //#include <Pms1Vme.h>
#define Pms2dHouse     NULL  //#include <Pms2dHouse.h>
#define PpsGps         NULL  //#include <PpsGps.h>
#include <RandomBuf.h>
#define Rdma           NULL  //#include <Rdma.h>
#include <SampleTable.h>
#define Spp            NULL  //#include <Spp.h>
#define Synchro        NULL  //#include <Synchro.h>
#include <TapeHeader.h>

class SyncRecord {
public:
  SyncRecord (TapeHeader& hdr, DsmConfig& dcfg, SampleTable& st,
/*               Bc635Vme& tp, */
/*               Counter *ctr, */
/*               DigitalIn *di, */
              Mensor *mensor[],
              Parsci *parsci[]
/*               Dsp56002 *dp, */
/*               HwIrs *hwirs[], */
/*               HwGps *hwgps[], */
/*               Mcr *mc, */
/*               Ophir3 *oph3[], */
/*               Pms1Vme *p1v[], */
/*               Pms2dHouse *tdh[], */
/*               GpsTans2 *tan2[], */
/*               GpsTans3 *tan3[], */
/*               Garmin *garmingps[],  */
/*               PpsGps *gpspps[], */
/*               Spp *serspp[], */
/*               UvHyg *uv[], */
/*               Vm3118 *vm3, */
/*               Synchro *synchro, */
/*               GreyVmeHouse *grayh[], */
/*               JplTdl *jpltdl[], */
/*               LhTdl *lhtdl, */
/*               Rdma *rdma, */
/*              Neph *neph[], */
/*               Cmigits3 *cmigits[], */
/*              Climet *climet */
/*               Mca *mca */
              );

  void buildRecord ();				// build the 1 second record
  RandomBuf *buf;				// logical record buffer
 
private:
  void buildHdr();				// build the HDR block
  void buildAnalog();				// build the Analog block
//  void buildAnaAux();				// build the Analog aux block
//  void buildCounter();			// build the Counters block
//  void buildCmigits3();			// build the Cmigits3 block
//  void buildDigitalIn();			// build the Digital inputs blk
//  void buildDigitalOut();			// build the Digital outputs blk
//  void buildDpres();				// build the Digital pressure 
  void buildMensor();				// build the Digital pressure 
  void buildParsci();				// build the Digital pressure 
//  void buildGpsTans();			// build the Trimble tans blks
//  void buildGarmin();				// build the Garmin gps blks
//  void buildHwIrs();				// build the Honeywell irs blks
//  void buildHwGps();				// build the Honeywell gps blks
//  void buildClimet();				// build the Climet block
//  void buildMca();				// build the Mca block
//  void buildNeph();				// build the Neph block
//  void buildPpsGps();				// build the Honeywell gps blks
//  void buildOphir3();				// build the Ophir3 blks
//  void buildOzone();				// build the NOAA ozone blks
//  void buildPms1Vme();			// build the vme pms 1d blocks
//  void buildRdma();				// build the Rdma block
//  void buildSpp();				// build spp block

  SampleTable &samp_table;			// sample table
//   Counter *counter;				// counters interface 
//   Cmigits3 *cmigits3;			// C-MIGITS 3 interfaces 
//   DigitalIn *digital_in;			// digital inputs interfaces 
/* Dpres *d_pres[MAX_UVHYG_INTFC];		// digital pressure interface */ 
     Mensor *men_pres[MAX_SER_INTFC];
     Parsci *par_pres[MAX_SER_INTFC];
/*   GpsTans3 *tans3[MAX_TANS_INTFC];		// Trimble Tans III intfcs  */
//   Garmin *garmin;				// Trimble Tans III intfcs  
//   HwIrs *hw_irs[MAX_HWIRS_INTFC];		// Honeywell irs interfaces  
//   HwGps *hw_gps[MAX_HWGPS_INTFC];		// Honeywell irs interfaces  
/*   PpsGps *pps_gps[MAX_PPS_INTFC];		// Collins PPS GPS interfaces */
/*   Mcr *mcr;					// Mcr interface */
//   Ophir3 *ophir3[MAX_OPHIR3_INTFC];		// Ophir3 interfaces  
/*   Pms1Vme *pms1v[MAX_PMS1VME_INTFC];		// pms 1d interfaces */
/*   Pms2dHouse *pms2dh[MAX_PMSVME2D_INTFC];  	// pms 2d interfaces */
/*   Synchro *synchro;				// Synchro interface */
//   Spp *spp[MAX_SPP_INTFC];			// Spp interfaces 
//   Climet *climet_cn;				// Climet 
/*   Mca *mca_cn;	 			// Mca  */
//   Neph *nephelometer[MAX_UVHYG_INTFC];		// Neph 
//   Rdma *radial_dma;				// Rdma 

  Hdr_blk hdr_blk;				// HDR block struct
  int lr_len;					// logical record length
};

#endif

