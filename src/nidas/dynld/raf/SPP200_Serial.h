/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision$

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_RAF_SPP200_SERIAL_H
#define NIDAS_DYNLD_RAF_SPP200_SERIAL_H

#include <nidas/dynld/raf/SppSerial.h>
#include <nidas/util/RunningAverage.h>

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
    DMT_UShort  trig_thresh;                    // trigger threshold
    DMT_UShort  chanCnt;                        // chanCnt
    DMT_UShort  range;                          // range
    DMT_UShort  avTranWe;                       // avgTransWeight
    DMT_UShort  divFlag;                        // divisorflag 0=/2, 1=/4
    DMT_UShort  OPCthreshold[MAX_CHANNELS];     // OPCthreshold[MAX_CHANNELS]
//    DMT_UShort  spares[4];
    DMT_UShort  chksum;                         // cksum
  };

//  static const int _InitPacketSize = 102;
  static const int _InitPacketSize = 94;

  /**
   * Data packet back from probe.  This is max size with 40 channels.
   * e.g. if 30 channels are requested, then this packet will be 40 bytes
   * shorter (10*sizeof(long)).
   */
  struct DMT200_blk
  {
      DMT_UShort cabinChan[8];
      DMT_UShort AvgTransit;
      DMT_UShort FIFOfull;
      DMT_UShort resetFlag;
      DMT_UShort SyncErrA;
      DMT_UShort SyncErrB;
      DMT_UShort SyncErrC;
      DMT_ULong ADCoverflow;
      DMT_ULong OPCchan[MAX_CHANNELS];	// 40 channels max
      DMT_UShort chksum;
  };

protected:

  inline int packetLen() const {
    return (34 + 4 * _nChannels);
  }

  static const size_t PHGB_INDX, PMGB_INDX, PLGB_INDX, PFLW_INDX, PREF_INDX,
	PFLWS_INDX, PTMP_INDX;

  nidas::util::RunningAverage<unsigned short, 44> _flowAverager;
  nidas::util::RunningAverage<unsigned short, 44> _flowsAverager;

};

}}}	// namespace nidas namespace dynld raf

#endif
