/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/CDP_Serial.h $

*/

#ifndef NIDAS_DYNLD_RAF_CDP_SERIAL_H
#define NIDAS_DYNLD_RAF_CDP_SERIAL_H

#include <nidas/dynld/raf/SppSerial.h>

#include <iostream>

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading PMS1D probes with the DMT interface conversion.
 * RS-422 @ 38400 baud.
 */
class CDP_Serial : public SppSerial
{
public:

  CDP_Serial();

  void fromDOMElement(const xercesc::DOMElement* node)
      throw(nidas::util::InvalidParameterException);

  void sendInitString() throw(nidas::util::IOException);

  bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();


  // Packet to initialize probe with.
  struct Init100_blk
  {
    char    esc;                                // ESC 0x1b
    char    id;                                 // cmd id
//    ushort  model;                              // model
    ushort  trig_thresh;                        // trigger threshold
    ushort  transRej;                           // Transit Reject
    ushort  chanCnt;                            // chanCnt
    ushort  dofRej;
    ushort  range;                              // range
    ushort  avTranWe;                           // avgTransWeight
    ushort  attAccept;
    ushort  divFlag;                            // divisorflag 0=/2, 1=/4
    ushort  ct_method;
//    ushort  max_width;                          // max_width threshold
    ushort  OPCthreshold[MAX_CHANNELS];         // OPCthreshold[MAX_CHANNELS]
    ushort  chksum;                             // cksum
  };

  /**
   * Data packet back from probe.  This is max size with 40 channels.
   * e.g. if 30 channels are requested, then this packet will be 40 bytes
   * shorter (10*sizeof(long)).
   */
  struct DMT100_blk
  {
      unsigned short cabinChan[8];
      unsigned long rejDOF;
      unsigned long rejAvgTrans;
      unsigned short AvgTransit;
      unsigned short FIFOfull;
      unsigned short resetFlag;
      unsigned long ADCoverflow;
      unsigned long OPCchan[MAX_CHANNELS];	// 40 channels max
      unsigned short chksum;
  };

protected:

  static const size_t FREF_INDX, FTMP_INDX;

  unsigned short _transitReject;
  unsigned short _dofReject;
  unsigned short _attAccept;
  unsigned short _ctMethod;

};

}}}	// namespace nidas namespace dynld raf

#endif
