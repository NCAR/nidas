/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-07-24 07:51:51 -0600 (Mon, 24 Jul 2006) $

    $LastChangedRevision: 3446 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/SppSerial.h $

*/

#ifndef NIDIS_DYNLD_RAF_SPPSERIAL_H
#define NIDIS_DYNLD_RAF_SPPSERIAL_H

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/util/EndianConverter.h>

namespace nidas { namespace dynld { namespace raf {

/**
 * Base class for many DMT Probes, including SPP100, SPP200, SPP300 and the CDP.
 */
class SppSerial : public DSMSerialSensor
{
public:

  SppSerial() : DSMSerialSensor() {}

  unsigned short computeCheckSum(unsigned char *pkt, int len);

  /**
   * Max # for array sizing.  Valid number of channels are 10, 20, 30 and 40.
   */
  static const int MAX_CHANNELS = 40;


protected:

  static const nidas::util::EndianConverter* toLittle;

  /**
   * Number of channels requested to be recorded.
   */
  size_t _nChannels;

  /**
   * Expected length of return data packet.
   */
  size_t _packetLen;

  /**
   * Total number of floats in the processed output sample.
   */
  size_t _noutValues;

  dsm_sample_id_t _sampleId;


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
