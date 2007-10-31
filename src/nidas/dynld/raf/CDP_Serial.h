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
  struct InitCDP_blk
  {
    char    esc;                                // ESC 0x1b
    char    id;                                 // cmd id
    DMT_UShort  trig_thresh;                    // trigger threshold
    DMT_UShort  transRej;                       // Transit Reject
    DMT_UShort  chanCnt;                        // chanCnt
    DMT_UShort  dofRej;
    DMT_UShort  range;                          // range
    DMT_UShort  avTranWe;                       // avgTransWeight
    DMT_UShort  attAccept;
    DMT_UShort  divFlag;                        // divisorflag 0=/2, 1=/4
    DMT_UShort  ct_method;
    DMT_UShort  OPCthreshold[MAX_CHANNELS];     // OPCthreshold[MAX_CHANNELS]
    DMT_UShort  chksum;                         // cksum
  };

  static const int _InitPacketSize = 102;

  /**
   * Data packet back from probe.  This is max size with 40 channels.
   * e.g. if 30 channels are requested, then this packet will be 40 bytes
   * shorter (10*sizeof(long)).
   */
  struct CDP_blk
  {
      DMT_UShort cabinChan[8];
      DMT_ULong rejDOF;
      DMT_ULong rejAvgTrans;
      DMT_UShort AvgTransit;
      DMT_UShort FIFOfull;
      DMT_UShort resetFlag;
      DMT_ULong ADCoverflow;
      DMT_ULong OPCchan[MAX_CHANNELS];	// 40 channels max
      DMT_UShort chksum;
  };

protected:

  inline int packetLen() const {
    return (36 + 4 * _nChannels);
  }

  static const size_t FLSR_CUR_INDX, FLSR_PWR_INDX;

  unsigned short _transitReject;
  unsigned short _dofReject;
  unsigned short _attAccept;
  unsigned short _ctMethod;
};

}}}	// namespace nidas namespace dynld raf

#endif
