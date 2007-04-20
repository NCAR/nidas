/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision$

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_RAF_SPPSERIAL_H
#define NIDAS_DYNLD_RAF_SPPSERIAL_H

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/util/EndianConverter.h>

namespace nidas { namespace dynld { namespace raf {

/**
 * Base class for many DMT Probes, including SPP100, SPP200, SPP300 and the CDP.
 */
class SppSerial : public DSMSerialSensor
{
public:

  SppSerial() : DSMSerialSensor() { _range = 0; }

  unsigned short computeCheckSum(const unsigned char *pkt, int len);


  static unsigned long fuckedUpLongFlip(const char * p);


  /**
   * Max # for array sizing.  Valid number of channels are 10, 20, 30 and 40.
   */
  static const int MAX_CHANNELS = 40;


protected:

  static const nidas::util::EndianConverter * toLittle;

  unsigned short _model;

  /**
   * Number of channels requested to be recorded.
   */
  int _nChannels;

  unsigned short _range;

  unsigned short _triggerThreshold;

  unsigned short _avgTransitWeight;

  unsigned short _divFlag;

  unsigned short _maxWidth;

  unsigned short _opcThreshold[MAX_CHANNELS];

  /**
   * Expected length of return data packet.
   */
  int _packetLen;

  /**
   * Total number of floats in the processed output sample.
   */
  int _noutValues;


  /**
   * Here more for documentation.  This is the data polling request packet.
   */
  struct reqPckt
  {
    char esc;		// 0x1b
    char id;		// 0x02
    ushort cksum;	// 0x001d
  };

};

}}}	// namespace nidas namespace dynld raf

#endif
