/* SyncRecord.cc
   Class for building the 1 second synchronous logical records.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#include <SyncRecord.h>
#include <DsmConfig.h>
#include <DsmMessage.h>


extern DsmMessage *comm_msg;
extern DsmConfig *dsm_config;

/******************************************************************************
** Public Functions
******************************************************************************/
 
SyncRecord::SyncRecord (TapeHeader& hdr, DsmConfig& dcfg, SampleTable& st, 
                        Mensor *presm[],
                        Parsci *presp[]
//                        Neph *neph[],
//                        Cmigits3 *cmigits[],
			) : samp_table (st)  
 
// Class constructor.
{
  int	j;
  
// Copy the passed in class pointers.
  
//   for (j = 0; j < MAX_CMIG_INTFC; j++)
//     cmigits3[j] = cmigits[j];

  for (j = 0; j < RTL_MAX_SERIAL; j++)
    men_pres[j] = presm[j];

   for (j = 0; j < RTL_MAX_SERIAL; j++)
     par_pres[j] = presp[j];

  lr_len = hdr.lrlen();			// keep a copy of the logical rec len

// Create the record buffer class.
  buf = new RandomBuf (lr_len);

// Init fixed fields in the HDR block.
  memset((char *)&hdr_blk, 0, sizeof(Hdr_blk));
  hdr_blk.id = SAMPLED_ID;
  strcpy(hdr_blk.dsm_locn, dcfg.location());

//   // Read TFP card.
//   hdr_blk.year = tfp.year();
//   hdr_blk.month = tfp.month();
//   hdr_blk.day = tfp.day();
//   hdr_blk.hour = tfp.hour();
//   hdr_blk.minute = tfp.minute();
//   hdr_blk.second = tfp.second();

}

/*****************************************************************************/
void SyncRecord::buildRecord ()

// Build the 1 second logical record.
{
  buildHdr();				// build the HDR block
  buildAnalog();			// enter the analog channels block
//  buildCounter();			// enter the counter channels block
//  buildCmigits3();			// enter the cmigits channels block
//  buildDigitalIn();			// enter the digital input chans block
//  buildDigitalOut();			// enter the digital output chans block
  buildMensor();                        // enter the digital pressure blocks
  buildParsci();                        // enter the digital pressure blocks
//  buildGarmin();			// enter the Garmin gps blocks
//  buildHwIrs();			// enter the Honeywell irs blocks
//  buildHwGps();			// enter the Honeywell gps blocks
//  buildOphir3();			// enter the Ophir3 blocks
//  buildClimet();			// enter the Climet block
//  buildNeph();                        // enter the Neph block
//  buildRdma();			// enter the rdma block
//  buildSpp();				// enter the Spp block 
  buf->setLen (lr_len);			// set buffer full
}
/******************************************************************************
** Private Functions
******************************************************************************/

void SyncRecord::buildHdr ()

// Builds the HDR block in the logical record.
{
  static int	firstTime = true;

  if (firstTime) {
//     hdr_blk.hour = tfp.hour();
//     hdr_blk.minute = tfp.minute();
//     hdr_blk.second = tfp.second();

    // Keep in mind you come in here AFTER the second is over, so subtract 1.

    if (--hdr_blk.second < 0)
      {
      hdr_blk.second = 59;
      if (--hdr_blk.minute < 0)
        {
        hdr_blk.minute = 59;
        if (--hdr_blk.hour < 0)
          hdr_blk.hour = 23;
        }
      }

    firstTime = false;
  }

//   if (garmin[0]) {
//     hdr_blk.year = garmin[0]->year();
//     hdr_blk.month = garmin[0]->month();
//     hdr_blk.day = garmin[0]->day();
//     }
//   else {
//     hdr_blk.year = tfp.year();
//     hdr_blk.month = tfp.month();
//     hdr_blk.day = tfp.day();
//     }


  // Copy to the record buffer.
  buf->putBuf ((char*)&hdr_blk, samp_table.startHdr(), samp_table.lenHdr());

//   hdr_blk.hour = tfp.hour();
//   hdr_blk.minute = tfp.minute();
//   hdr_blk.second = tfp.second();
  ++hdr_blk.rec_cnt;

/**
  printf ("SyncRecord: %02d/%02d/%02d, %02d:%02d:%02d\n", 
          hdr_blk.month, hdr_blk.day, hdr_blk.year, hdr_blk.hour, 
          hdr_blk.minute, hdr_blk.second);
**/
}
/*****************************************************************************/

void SyncRecord::buildAnalog ()

// Enters the analog block into the logical record.
{
//   int len;
//   int idx;
//   int off;
//   int j;
//   char *src;

//   if ((len = samp_table.ana_table.lenhigh()) && dsp->high_rate() == 1000)
//     buf->putBuf (dsp->buf1000(), samp_table.ana_table.starthigh(), 
//                  len * RATE_1000); 

//   if (len = samp_table.ana_table.len250())	// 250 hz block
//     buf->putBuf (dsp->buf250(), samp_table.ana_table.start250(),
//                  len * RATE_250); 

// // The 25 and 5 hz analog blocks are multiplexed with the analog auxiliary,
// // digital, and counter 25 and 5 hertz blocks.
//   if (len = samp_table.ana_table.len25()) { 	// 25 hz block
//     for (src = dsp->buf25(), idx = samp_table.ana_table.start25(), 
//          off = samp_table.ana_table.offset25(), j = 0; j < RATE_25;
//          src += len, idx += off, j++) {
//       buf->putBuf (src, idx, len); 
// //      if (j == 0) logMsg("*src = %x %x\n",*(short*)src, *(short*)(src + 2),
// //            0,0,0,0);
//     }
//   }

//   if (len = samp_table.ana_table.len5()) {	// 5 hz block
//     for (src = dsp->buf5(), idx = samp_table.ana_table.start5(), 
//          off = samp_table.ana_table.offset5(), j = 0; j < RATE_5;
//          src += len, idx += off, j++)
//       buf->putBuf (src, idx, len); 
//   }
}
/*****************************************************************************/

//void SyncRecord::buildCmigits3 ()

// Enters the Cmigits blocks into the logical record.
//{
//   int stat;
//   int j;
//   Cmigits3_blk *cmigits3_blk;

//   for (j = 0, stat = samp_table.cmigits3_table.firstEntry(); stat;
//        j++, stat = samp_table.cmigits3_table.nextEntry()) {

//     while(!cmigits3[j]->Cmig_sem);

//     buf->putBuf (cmigits3[j]->buffer(),
//                  samp_table.cmigits3_table.start(),
//                  samp_table.cmigits3_table.length());

//     cmigits3[j]->Cmig_sem = FALSE;

// //    cmigits3_blk = (Cmigits3_blk*)cmigits3[j]->buffer();
//    }
//}
/*****************************************************************************/
 
//void SyncRecord::buildCounter ()
 
// Enters the counter block into the logical record.
//{
//   int len;
//   int idx;
//   int off;
//   int j;
//   char *src;
 
// // The 25 and 5 hz counter blocks are multiplexed with the analog, analog
// // auxiliary, and digital 25 and 5 hertz blocks.
//   if (len = samp_table.counter_table.len25()) {		// 25 hz block
//     for (src = counter->buf25(), idx = samp_table.counter_table.start25(), 
//          off = samp_table.counter_table.offset25(), j = 0; j < RATE_25;
//          src += len, idx += off, j++)
//       buf->putBuf (src, idx, len); 
//   }
 
//   if (len = samp_table.counter_table.len5()) {     	// 5 hz block
//     for (src = counter->buf5(), idx = samp_table.counter_table.start5(), 
//          off = samp_table.counter_table.offset5(), j = 0; j < RATE_5;
//          src += len, idx += off, j++)
//       buf->putBuf (src, idx, len); 
//   }
 
//   if (len = samp_table.counter_table.len1())           	// 1 hz block
//     buf->putBuf (counter->buf1(), samp_table.counter_table.start1(), len);
//}
/*****************************************************************************/
 
//void SyncRecord::buildDigitalIn ()
 
// Enters the digital inputs block into the logical record.
//{
//   int len;
//   int idx;
//   int off;
//   int j;
//   char *src;

// // The 55, 50, 25 and 5 hz digital blocks are multiplexed with the analog, 
// // analog auxiliary, and counter 25 and 5 hertz blocks.
//   if (len = samp_table.dig_in_table.len55()) {          // 55 hz block
//     for (src = digital_in->buf55(), idx = samp_table.dig_in_table.start55(),
//          off = samp_table.dig_in_table.offset55(), j = 0; j < RATE_55;
//          src += len, idx += off, j++) {
//       buf->putBuf (src, idx, len);
//     }
//   }

//   if (len = samp_table.dig_in_table.len50()) {          // 50 hz block
//     for (src = digital_in->buf50(), idx = samp_table.dig_in_table.start50(),
//          off = samp_table.dig_in_table.offset50(), j = 0; j < RATE_50;
//          src += len, idx += off, j++) {
//       buf->putBuf (src, idx, len);
// //    logMsg ("*src = %x %x\n", *(short*)src, *(short*)(src + 2), 0,0,0,0);
//     }
//   }

//   if (len = samp_table.dig_in_table.len25()) {		// 25 hz block
//     for (src = digital_in->buf25(), idx = samp_table.dig_in_table.start25(), 
//          off = samp_table.dig_in_table.offset25(), j = 0; j < RATE_25;
//          src += len, idx += off, j++) {
//       buf->putBuf (src, idx, len); 
// //    logMsg ("*src = %x %x\n", *(short*)src, *(short*)(src + 2), 0,0,0,0);
//     }
//   }
 
//   if (len = samp_table.dig_in_table.len5()) {     	// 5 hz block
//     for (src = digital_in->buf5(), idx = samp_table.dig_in_table.start5(), 
//          off = samp_table.dig_in_table.offset5(), j = 0; j < RATE_5;
//          src += len, idx += off, j++) {
// //      printf("Dig_8 data = 0x%x, %d\n",*(short*)src, *(short*)src);
//       buf->putBuf (src, idx, len); 
//     }
//   }

//   if (len = samp_table.dig_in_table.len1())           	// 1 hz block
//     buf->putBuf (digital_in->buf1(), samp_table.dig_in_table.start1(), len);
//}
/*****************************************************************************/
 
//void SyncRecord::buildDigitalOut ()

// Enters the digital output words into the logical record.
//{
//   short out;

// // Enter the current MCR output value.
//   if (samp_table.mcr_table.startMcrOut()) {
//     out = (short)mcr->curOut();
//     buf->putBuf ((char*)&out, samp_table.mcr_table.startMcrOut(), 
//                  sizeof (short));
//   }
//}
/*****************************************************************************/
 
//void SyncRecord::buildGarmin ()
 
// Enters the Garmin blocks into the logical record.
//{
//   int stat;
//   int j;
//   Garmin_blk *garmin_blk;
 
//   for (j = 0, stat = samp_table.garmin_table.firstEntry(); stat;
//        j++, stat = samp_table.garmin_table.nextEntry()) {
//     buf->putBuf (garmin[j]->buffer(),
//                  samp_table.garmin_table.start(),
//                  samp_table.garmin_table.length());


// //    garmin_blk = (Garmin_blk*)garmin[j]->buffer();
// /*
//     printf ("GARMIN: glat = %f, glon = %f, galt = %f, utctime = %s\n",
//             garmin_blk->glat, garmin_blk->glon, garmin_blk->height,
//             garmin_blk->utctime);
// */
// //    printf ("GARMIN: status = %c\n",garmin_blk->status);
// //    printf ("GARMIN: Gspd = %f, Course = %f\n",garmin_blk->ground_speed,
// //	     garmin_blk->course);

//   }
//}
/*****************************************************************************/
 
void SyncRecord::buildMensor ()
 
// Enters the digital pressure block into the logical record.
{
  int stat;
  int j;
//   Mensor_blk *presm_blk;
 
  for (j = 0, stat = samp_table.mensor_table.firstEntry(); stat;
       j++, stat = samp_table.mensor_table.nextEntry()) {
    buf->putBuf(men_pres[j]->buffer(),
		samp_table.mensor_table.start(),
		samp_table.mensor_table.length());
  }

//  presm_blk = (Mensor_blk*)men_pres[0]->buffer();
}
/*****************************************************************************/
 
void SyncRecord::buildParsci ()
 
// Enters the Parascientific block into the logical record.
{
   int stat;
   int j;
//   Parsci_blk *p;

   for (j = 0, stat = samp_table.parsci_table.firstEntry(); stat;
        j++, stat = samp_table.parsci_table.nextEntry()) {
     buf->putBuf(par_pres[j]->buffer(),
                 samp_table.parsci_table.start(),
                 samp_table.parsci_table.length());
//    par_pres[j]->zero();
   }
//  p = (Parsci_blk *)par_pres[0]->buffer();
 
}

/*****************************************************************************/

//void SyncRecord::buildClimet ()

// Enters the Climet block into the logical record.
//{

//  Climet_blk *p;

//  if (climet_cn) {
//    buf->putBuf((char *)(p = (Climet_blk *)climet_cn->buffer()),
//		samp_table.climet_table.start(),
//		samp_table.climet_table.length());
//  }

//}
/*****************************************************************************/

//void SyncRecord::buildRdma ()

// Enters the Rdma block into the logical record.
//{

//   Rdma_blk *p; 

//   if (radial_dma) {
//     buf->putBuf((char *)(p = (Rdma_blk *)radial_dma->buffer()),
//                 samp_table.rdma_table.start(),
//                 samp_table.rdma_table.length());
// // printf("%f  %f %f\n", p->scan[0],p->scan[1],p->scan[2]);
// // printf("%s\n", p->item_type);
//   }

//}

/*****************************************************************************/

//void SyncRecord::buildHwIrs ()
 
// Enters the Honeywell irs blocks into the logical record.
//{
//   int stat;
 
// // Copy the blocks. 
//   for (stat = samp_table.hwirs_table.firstEntry(); stat;
//        stat = samp_table.hwirs_table.nextEntry()) {
 
//     buf->putBuf(hw_irs[samp_table.hwirs_table.index()]->buffer(),
//                 samp_table.hwirs_table.start(),
//                 samp_table.hwirs_table.length());
// /**
//     Irs_blk *irsP = (Irs_blk*)hw_irs[samp_table.hwirs_table.index()]->buffer();
//     printf ("irsP = 0x%X, lag50 = %d, lag25 = %d, lag10 = %d, lag5 = %d\n",
//         irsP,
//         irsP->lag_50hz_frame, irsP->lag_25hz_frame,
//         irsP->lag_10hz_frame, irsP->lag_5hz_frame);
// **/
//   }
//}

/*****************************************************************************/
//void SyncRecord::buildHwGps ()
 
// Enters the Honeywell gps blocks into the logical record.
//{
//   int stat;
//   HwGps_blk *hgps_blk;
 
// // Copy the blocks.
//   for (stat = samp_table.hwgps_table.firstEntry(); stat;
//        stat = samp_table.hwgps_table.nextEntry()) {
 
//     buf->putBuf (hw_gps[samp_table.hwgps_table.index()]->buffer(),
//                  samp_table.hwgps_table.start(),
//                  samp_table.hwgps_table.length());
// /**
//     hgps_blk = (HwGps_blk*)hw_gps[samp_table.hwgps_table.index()]->buffer();
//     printf ("hgps_blk = 0x%X, lag50 = %d, lag25 = %d, lag10 = %d, lag5 = %d\n",
//         hgps_blk);
// **/
//   }
//}
/*****************************************************************************/
 
//void SyncRecord::buildOphir3 ()
 
// Enters the Ophir3 blocks into the logical record.
//{
//   int stat;
//   int j;
 
//   for (j = 0, stat = samp_table.ophir3_table.firstEntry(); stat;
//        j++, stat = samp_table.ophir3_table.nextEntry()) {
//     buf->putBuf (ophir3[j]->buffer(),
//                  samp_table.ophir3_table.start(),
//                  samp_table.ophir3_table.length());
 
// //  ophir3[j]->display();
//   }
//}
/*****************************************************************************/

//void SyncRecord::buildSpp ()

// Enters the Spp blocks into the logical record.
//{
//   int stat;
//   int j;
//   DMT100_blk *p;

//   for (j = 0, stat = samp_table.spp_table.firstEntry(); stat;
//        j++, stat = samp_table.spp_table.nextEntry()) {
//     buf->putBuf ((char *)(p = (DMT100_blk *)spp[j]->buffer()),
//                  samp_table.spp_table.start(),
//                  samp_table.spp_table.length());
//  printf("%d %d %d %d\n", p->OPCchan[2], p->OPCchan[12], p->OPCchan[22], p->OPCchan[32]);
//   }
//}

/*****************************************************************************/

//void SyncRecord::buildNeph ()

// Enters the Neph block into the logical record.
//{

//  int j, stat;

//  for (j = 0, stat = samp_table.neph_table.firstEntry(); stat;
//        j++, stat = samp_table.neph_table.nextEntry()) {
//    buf->putBuf(nephelometer[j]->buffer(),
//                samp_table.neph_table.start(),
//                samp_table.neph_table.length());
//  }
//}

