/* SampleTable.cc
   Class for managing the DSM sample table.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#include <SampleTable.h>

/******************************************************************************
** Public Functions
******************************************************************************/

void SampleTable::buildTable  ()

// Build the sample table from the tape header.
{
  int j;
  int stat;
//   int tans_count = 0;
//   int garmin_count = 0;
//   int ophir_count = 0;
//   int uvh_count = 0;
//   int pps_count = 0;
//  int dpres_count = 0;
  int mensor_count = 0;
  int parsci_count = 0;
//   int aerosol_count = 0;
//   int mass_count = 0;
//   int jpltdl_count = 0;
//   int lhtdl_count = 0;
//  int climet_count = 0;
//   int mca_count = 0;
//  int neph_count = 0;
//   int rdma_count = 0;
//   int cmigits3_count = 0;

// Parse through the header building the sample tables. 
  dsm_hdr.firstDesc();
  for (stat = dsm_hdr.nextDesc(); stat; stat = dsm_hdr.nextDesc()) {

    printf("SampleTable::buildTable - dsm_hdr.getDesc() = %s\n", dsm_hdr.getDesc() );

// Until xbuild incorporates ANALOG_AUX_STR, use a check on the address to
// determine if this is a standard analog input or an auxiliary input.
    if (dsm_hdr.analogType()) {
//       ana_table.enter (dsm_hdr.adsaddr(), dsm_hdr.rate(), dsm_hdr.ch_gain(),
//                        dsm_hdr.ch_offset(), dsm_hdr.start(), dsm_hdr.offset(), 
//                        dsm_hdr.length());
    }
    else if (dsm_hdr.anaAuxType()) {
//       ana_aux_table.enter (dsm_hdr.adsaddr(), dsm_hdr.rate(), dsm_hdr.ch_gain(),
//                            dsm_hdr.start(), dsm_hdr.offset(), dsm_hdr.length());
    }
    else if (dsm_hdr.synchroType()) {
//       synchro_table.enter (dsm_hdr.adsaddr(), dsm_hdr.rate(),
//                            dsm_hdr.start(), dsm_hdr.offset(), dsm_hdr.length());
    }
    else if (dsm_hdr.counterType()) {
//       counter_table.enter (dsm_hdr);
    }
    else if (dsm_hdr.digitalType()) {
//       dig_in_table.enter (dsm_hdr);
//       if (!strcmp (dsm_hdr.name(), MCRSTAT_NAME))
//         mcr_table.enterMcrStat (dsm_hdr);
    }
    else if (dsm_hdr.digoutType()) {
//       if (!strcmp (dsm_hdr.name(), MCROUT_NAME))
//         mcr_table.enterMcrOut (dsm_hdr);
    }
    else if (dsm_hdr.evtType()) {
      j++;
    }
    else if (dsm_hdr.gpsTans2Type()) {
// // For now use the count for the location until xbuild gets locations added.
//       sprintf (tstr, "%1s", tans_count + 1);
//       dsm_hdr.setBlkLocn (tstr);
//       tans2_table.enter (dsm_hdr, TANS_PORT_1 + tans_count++);
    }
    else if (dsm_hdr.gpsTans3Type()) {
//       tans3_table.enter (dsm_hdr, TANS_PORT_1 + tans_count++);
    }
    else if (dsm_hdr.garminType()) {
//       garmin_table.enter (dsm_hdr, GARMIN_PORT_1 + garmin_count++);
    }
    else if (dsm_hdr.cmigits3Type()) {
//       cmigits3_table.enter (dsm_hdr, CMIGITS3_PORT_1 + cmigits3_count++);
    }
    else if (dsm_hdr.dpresType()) { 
//      dpres_table.enter (dsm_hdr, DPRES_PORT_1 + dpres_count++);
    }
    else if (dsm_hdr.mensorType()) { 
      mensor_table.enter (dsm_hdr, MENSOR_PORT_1 + mensor_count++);
    }
    else if (dsm_hdr.parsciType()) { 
      parsci_table.enter (dsm_hdr, PARSCI_PORT_1 + parsci_count++);
    }
    else if (dsm_hdr.ppsGpsType()) { 
//       pps_table.enter (dsm_hdr, PPSGPS_PORT_1 + pps_count++);
    }
    else if (dsm_hdr.aerosolType()) { 
//       aerosol_table.enter (dsm_hdr, AEROSOL_PORT_1 + aerosol_count++);
    }
    else if (dsm_hdr.masspecType()) { 
//       masspec_table.enter (dsm_hdr, MASSPEC_PORT_1 + mass_count++);
    }
    else if (dsm_hdr.jpltdlType()) { 
//       jpltdl_table.enter (dsm_hdr, JPLTDL_PORT_1 + jpltdl_count++);
    }
    else if (dsm_hdr.lhtdlType()) { 
//       lhtdl_table.enter (dsm_hdr, LHTDL_PORT_1 + lhtdl_count++);
    }
    else if (dsm_hdr.climetType()) {
//      climet_table.enter (dsm_hdr, CLIMET_PORT_1 + climet_count++);
    }
    else if (dsm_hdr.mcaType()) {
//       mca_table.enter (dsm_hdr, MCA_PORT_1 + mca_count++);
    }
    else if (dsm_hdr.nephType()) {
//      neph_table.enter (dsm_hdr, NEPH_PORT_1 + neph_count++);
    }
    else if (dsm_hdr.rdmaType()) {
//       rdma_table.enter (dsm_hdr, RDMA_PORT_1 + rdma_count++);
    }
    else if (dsm_hdr.hdrType()) {
      enterHdr ();
    }
    else if (dsm_hdr.hskpType()) {
//      j++;
    }
    else if (dsm_hdr.insType()) {
//      j++;
    }
    else if (dsm_hdr.irsType()) {
//       hwirs_table.enter (dsm_hdr);
    }
    else if (dsm_hdr.hwGpsType()) {
//       hwgps_table.enter (dsm_hdr);
    }
    else if (dsm_hdr.lrncType()) {
//      j++;
    }
    else if (dsm_hdr.ophir3Type()) {
// // For now use the count for the location until xbuild gets locations added.
//       sprintf (tstr, "%1s", ophir_count + 1);
//       dsm_hdr.setBlkLocn (tstr);
//       ophir3_table.enter (dsm_hdr, OPHIR_PORT_1 + ophir_count++);
    }
    else if (dsm_hdr.pms1Type()) {
//      j++;
    }
    else if (dsm_hdr.pms1v2Type()) {
//       pms1vme_table.enter (dsm_hdr);
    }
    else if (dsm_hdr.sppType()) {
//       spp_table.enter (dsm_hdr, dsm_hdr.spp_intf_chan());
    }
    else if (dsm_hdr.serialType()) {
      j++;
    }
    else if (dsm_hdr.uvhygType()) {
// // For now use the count for the location until xbuild gets locations added.
//       sprintf (tstr, "%1s", uvh_count + 1);
//       dsm_hdr.setUvhygLocn (tstr);
//       uvh_table.enter (dsm_hdr, UVHYG_PORT_1 + uvh_count++);
    }
    else if (dsm_hdr.pms2Type()) {
//       if (!strcmp (dsm_hdr.name(), P2D_G_STR)) {
//         greyvme_table.enter (dsm_hdr);
//       }
//       else {
//         pms2d_table.enter (dsm_hdr);
//       }
    }
    else if (dsm_hdr.pms2hType()) {
//       if (!strcmp (dsm_hdr.name(), P2D_G_STR)) {
//         greyh_table.enter (dsm_hdr);
//       }
//       else {
//         pms2dh_table.enter (dsm_hdr);
//       }
    }
    else if (dsm_hdr.asyType()) {
//      j++;
    }
    else {
      fprintf (stderr, "SampleTable: Unknown tape header descriptor --> [%s]\n",
               dsm_hdr.item_type());
      exit (ERROR);
    }
  }

// Display the setup tables.
/*
  displayFlightDesc();
  printf ("Analog Inputs\n");
  ana_table.displayTable();
  printf ("Analog Auxiliary Inputs\n");
  ana_aux_table.displayTable();
  printf ("Counter Inputs\n");
  counter_table.displayTable();
  printf ("Digital Inputs\n");
  dig_in_table.displayTable();
  printf ("Honeywell IRS\n");
  hwirs_table.displayTable();
  printf ("MCR\n");
  mcr_table.displayTable();
  printf ("Ophir 3\n");
  ophir3_table.displayTable();
  printf ("Pms 1D\n");
  pms1vme_table.displayTable();
  printf ("Trimble Tans 2\n");
  tans2_table.displayTable();
  printf ("Trimble Tans 3\n");
  tans3_table.displayTable();
  printf ("Collins PPS GPS\n");
  garmin_table.displayTable();
  printf ("GARMIN GPS\n");
  cmigits3_table.displayTable();
  printf ("CMIGITS3\n");
  pps_table.displayTable();
  printf ("UV Hygrometer\n");
  uvh_table.displayTable();
  printf ("JPL TDL\n");
  jpltdl_table.displayTable();
  printf ("Pms 2D\n");
  pms2d_table.displayTable();
  pms2dh_table.displayTable();
  greyh_table.displayTable();
  greyvme_table.displayTable();
*/
}
/*****************************************************************************/

int SampleTable::startHdr  ()

// Returns the HDR block offset.
{
  return start_hdr;
}
/*****************************************************************************/

int SampleTable::lenHdr  ()

// Returns the HDR block length.
{
  return len_hdr;
}
/******************************************************************************
** Private Functions
******************************************************************************/

void SampleTable::enterHdr  ()

// Enters the HDR block sample table entries.
{ 
  start_hdr = dsm_hdr.start();            // offset to the block
  len_hdr = dsm_hdr.length();             // number of bytes in the block

//  printf ("start_hdr = %d, len_hdr = %d\n", start_hdr, len_hdr);
}
/*****************************************************************************/

void SampleTable::displayFlightDesc ()

// Displays the flight descriptor information.
{
//  printf ("lr_len = %d\n", dsm_hdr.lrlen());
}
/*****************************************************************************/

