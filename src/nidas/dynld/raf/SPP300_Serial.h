// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/SPP300_Serial.h $

*/

#ifndef NIDAS_DYNLD_RAF_SPP300_SERIAL_H
#define NIDAS_DYNLD_RAF_SPP300_SERIAL_H

#include <nidas/dynld/raf/SppSerial.h>

#include <iostream>

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading PMS1D probes with the DMT interface conversion.
 * RS-422 @ 38400 baud.
 */
class SPP300_Serial : public SppSerial
{
public:

  SPP300_Serial();

  void fromDOMElement(const xercesc::DOMElement* node)
      throw(nidas::util::InvalidParameterException);

  void sendInitString() throw(nidas::util::IOException);

  bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();


  // Packet to initialize probe with.
  struct Init300_blk
  {
    char    esc;				// ESC 0x1b
    char    id;					// cmd id
    DMT_UShort  trig_thresh;			// trigger threshold
    DMT_UShort  chanCnt;			// chanCnt
    DMT_UShort  dofRej;
    DMT_UShort  range;				// range
    DMT_UShort  avTranWe;			// avgTransWeight
    DMT_UShort  divFlag;			// divisorflag 0=/2, 1=/4
    DMT_UShort  OPCthreshold[MAX_CHANNELS];	// OPCthreshold[MAX_CHANNELS]
    DMT_UShort  chksum;				// cksum
  };

  static const int _InitPacketSize = 96;

  /**
   * Data packet back from probe.  This is max size with 40 channels.
   * e.g. if 30 channels are requested, then this packet will be 40 bytes
   * shorter (10*sizeof(long)).
   */
  struct DMT300_blk
  {
      DMT_UShort cabinChan[8];
      DMT_ULong rejDOF;
      DMT_UShort AvgTransit;
      DMT_UShort FIFOfull;
      DMT_UShort resetFlag;
      DMT_UShort SyncErrA;
      DMT_UShort SyncErrB;
      DMT_ULong ADCoverflow;
      DMT_ULong OPCchan[MAX_CHANNELS];	// 40 channels max
      DMT_UShort chksum;
  };

protected:
  int packetLen() const {
    return (36 + 4 * _nChannels);
  }

  static const size_t FREF_INDX, FTMP_INDX;

  unsigned short _dofReject;

};

}}}	// namespace nidas namespace dynld raf

#endif
