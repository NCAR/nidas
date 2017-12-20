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

#ifndef NIDAS_DYNLD_RAF_PIP_SERIAL_H
#define NIDAS_DYNLD_RAF_PIP_SERIAL_H

#include "SppSerial.h"
#include <nidas/core/DerivedDataClient.h>
#include <iostream>

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading DMT PIP/CIP probe histogram data.
 * RS-422 @ 38400 baud.
 */
class PIP_Serial : public SppSerial, public DerivedDataClient
{
public:

  PIP_Serial();

  void validate()
        throw(nidas::util::InvalidParameterException);

  void sendInitString() throw(nidas::util::IOException);

  bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

  static const size_t N_PIP_CHANNELS = 62;
  static const size_t N_PIP_HSKP = 16;

  // Packet to initialize probe with.
  struct InitPIP_blk
  {
    unsigned char   esc;               // ESC 0x1b
    unsigned char   id;                // cmd id
    DMT_UShort      airspeedSource;    // PAS
    DMT_UShort      dofRej;
    unsigned char   pSizeDim;          // ParticleSizingDimension
    unsigned char   rc;                // recovery coefficient
    DMT_UShort      chksum;            // cksum
  };
 
  static const int _InitPacketSize = 10;
  static const int _setTimePacketSize = 8;

  /**
   * Packet sent to probe to begin sending data. 
   * Usually this is put in the xml, but because
   * PASCoefficient has to be calculated by the
   * server, and thus is not static, is here.
   * Don't actually know that it'll be used though.
   */
  struct SendPIP_BLK
  {
    unsigned char esc;
    unsigned char id;                  // ESC 0x1b id =0x02
    DMT_UShort  hostSyncCounter;
    DMT_ULong PASCoefficient;
    DMT_UShort  relayControl;
    DMT_UShort  chksum;                         // cksum
  };

  static const int _SendDataPacketSize = 12;
  struct SetAbsoluteTime
  {
    unsigned char esc;
    unsigned char id; //5
    unsigned char sec; //sec/milisec may need to be swapped
    unsigned char milisec; 
    unsigned char hour; //hour/min may need to be swapped
    unsigned char min; 
    //DMT_UShort secMili; //seconds and miliseconds
   // DMT_UShort hourMin; //set hour and min
    DMT_UShort chksum;
  };

  /**
   * Data packet back from probe.
   */
  struct PIP_blk
  {
      unsigned char header1;
      unsigned char header2;
      DMT_UShort packetByteCount;
      DMT_UShort oversizeReject;
      DMT_UShort binCount[N_PIP_CHANNELS];
      DMT_UShort DOFRejectCount;
      DMT_UShort EndRejectCount; 
      DMT_UShort housekeeping[N_PIP_HSKP];
      DMT_UShort ParticleCounter;
// mixing types here to see what actually comes out of all this   
//      unsigned char sec; //sec/milisec may need to be swapped
//    unsigned char milisec; 
      DMT_UShort SecMili; //Seconds and Milliseconds
      unsigned char hour; // hour/min may need to be swapped
      unsigned char min; 
    //  DMT_UShort SecMili; //Seconds and Milliseconds
    //  DMT_UShort HourMin; //Hour and minute
      DMT_UShort hostSyncCounter; 
      DMT_UShort resetFlag;
      DMT_UShort chksum;
      unsigned char trailer1;
      unsigned char trailer2;
  };
   



    /**
     *  PIP has dynamic TAS
     */
    virtual void
    derivedDataNotify(const nidas::core:: DerivedDataReader * s)
        throw();
    /**
     * open the sensor and perform any intialization to the driver.
     */
    void open(int flags)
        throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    void close() throw(nidas::util::IOException);

protected:

    int packetLen() const {
        return (180);    //use _nChannels if binCount ends up being variable
    }
  
    /**
     * Set probe time.
     */
    void sendTimePacket()throw(nidas::util::IOException);

    int appendDataAndFindGood(const Sample* sample);
  
    // These are instantiated in .cc, used for indexing into the housekeeping array
    static const size_t PIPEDV0,PIPEDV64,PIPEDV32,PIPQC,PIPPS,PIPLWC,PIPLWCSLV,
            PIPCBTMP,PIPRH,PIPRT,PIPLSRC,PIPLSRP,REJOFLOW,REJDOF,REJEND;

    unsigned short _dofReject;
    unsigned short _airspeedSource;

    /**
     * True air speed, received from IWGADTS feed.
     */
    float _trueAirSpeed;
};

}}}	// namespace nidas namespace dynld raf

#endif
