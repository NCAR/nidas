/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-07-24 07:51:51 -0600 (Mon, 24 Jul 2006) $

    $LastChangedRevision: 3446 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/SPP200_Serial.h $

*/

#ifndef NIDIS_DYNLD_RAF_SPP200_SERIAL_H
#define NIDIS_DYNLD_RAF_SPP200_SERIAL_H

#include <nidas/dynld/raf/SppSerial.h>

#include <iostream>

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading PMS1D probes with the DMT interface conversion.
 * RS-422 @ 38400 baud.
 */
class SPP200_Serial : public SppSerial
{
public:

  SPP200_Serial();

  void fromDOMElement(const xercesc::DOMElement* node)
      throw(nidas::util::InvalidParameterException);

  void sendInitString() throw(nidas::util::IOException);

  bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();


  // Packet to initialize probe with.
  struct Init200_blk
  {
    char    esc;                                // ESC 0x1b
    char    id;                                 // cmd id
    ushort  model;                              // model
    ushort  trig_thresh;                        // trigger threshold
    ushort  chanCnt;                            // chanCnt
    ushort  range;                              // range
    ushort  avTranWe;                           // avgTransWeight
    ushort  divFlag;                            // divisorflag 0=/2, 1=/4
    ushort  max_width;                          // max_width threshold
    ushort  OPCthreshold[MAX_CHANNELS];         // OPCthreshold[MAX_CHANNELS]
    ushort  spares[4];
    ushort  chksum;                             // cksum
  };

  /**
   * SP200 response adds a firmware field in the middle
   */
  struct Response200_blk
  {
    char    esc;                                // ESC 0x1b
    char    id;                                 // cmd id
    ushort  model;                              // model
    ushort  firmware;                           // firmware
    ushort  trig_thresh;                        // trigger threshold
    ushort  chanCnt;                            // chanCnt
    ushort  range;                              // range
    ushort  avTranWe;                           // avgTransWeight
    ushort  divFlag;                            // divisorflag 0=/2, 1=/4
    ushort  max_width;                          // max_width threshold
    ushort  OPCthreshold[MAX_CHANNELS];         // OPCthreshold[MAX_CHANNELS]
    ushort  spares[4];
    ushort  chksum;                             // cksum
  };

  /**
   * Data packet back from probe.  This is max size with 40 channels.
   * e.g. if 30 channels are requested, then this packet will be 40 bytes
   * shorter (10*sizeof(long)).
   */
  struct DMT200_blk
  {
      unsigned short cabinChan[8];
      unsigned short AvgTransit;
      unsigned short FIFOfull;
      unsigned short resetFlag;
      unsigned short SyncErrA;
      unsigned short SyncErrB;
      unsigned short SyncErrC;
      unsigned long ADCoverflow;
      unsigned long OPCchan[MAX_CHANNELS];	// 40 channels max
      unsigned short chksum;
  };

private:


};

}}}	// namespace nidas namespace dynld raf

#endif
