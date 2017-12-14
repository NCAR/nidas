// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#ifndef NIDAS_DYNLD_RAF_SPP300_SERIAL_H
#define NIDAS_DYNLD_RAF_SPP300_SERIAL_H

#include "SppSerial.h"

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading PMS1D probes with the DMT interface conversion.
 * RS-422 @ 38400 baud.
 */
class SPP300_Serial : public SppSerial
{
public:

  SPP300_Serial();

  void validate() throw(nidas::util::InvalidParameterException);

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

  unsigned short _divFlag;

  unsigned short _avgTransitWeight;

  unsigned short _dofReject;

};

}}}	// namespace nidas namespace dynld raf

#endif
