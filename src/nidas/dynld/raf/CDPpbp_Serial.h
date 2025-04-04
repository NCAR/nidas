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

#ifndef NIDAS_DYNLD_RAF_CDPpbp_SERIAL_H
#define NIDAS_DYNLD_RAF_CDPpbp_SERIAL_H

#include "CDP_Serial.h"

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading PMS1D probes with the DMT interface conversion.
 * RS-422 @ 38400 baud.
 */
class CDPpbp_Serial : public CDP_Serial
{
public:

  CDPpbp_Serial();

  /**
   * @throw()
   **/
  bool process(const Sample* samp,std::list<const Sample*>& results);


  /**
   * Data packet back from probe.  This is max size with 40 channels.
   * e.g. if 30 channels are requested, then this packet will be 40 bytes
   * shorter (10*sizeof(long)).
   */
  struct CDPpbp_blk
  {
      DMT_UShort cabinChan[8];
      DMT_ULong rejDOF;
      DMT_UShort QualBndwdth;
      DMT_UShort QualThrshld;
      DMT_UShort AvgTransit;
      DMT_UShort SizerBndwdth;  // in 2012 documentation this is Sizer DT Bandwidth (unused)
      DMT_UShort SizerThrshld;  // in 2012 documentation this is Sizer Dynamic Threshold (unused)
      DMT_ULong ADCoverflow;
      DMT_ULong OPCchan[MAX_CHANNELS];	// 40 channels max
      unsigned char pbp[1030];
      DMT_UShort chksum;
  };

protected:

  int packetLen() const {
    return (36 + (4 * _nChannels) + 1030);
  }

};

}}}	// namespace nidas namespace dynld raf

#endif
