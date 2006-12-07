/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-07-24 07:51:51 -0600 (Mon, 24 Jul 2006) $

    $LastChangedRevision: 3446 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/SPP200_Serial.h $

*/

#ifndef NIDIS_DYNLD_RAF_SPP200_SERIAL_H
#define NIDIS_DYNLD_RAF_SPP200_SERIAL_H

#include <nidas/dynld/DSMSerialSensor.h>

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading PMS1D probes with the DMT interface conversion.
 * RS-422 @ 38400 baud.
 */
class SPP200_Serial : public DSMSerialSensor
{
public:

  SPP200_Serial() : DSMSerialSensor() {}

  void fromDOMElement(const xercesc::DOMElement* node)
      throw(nidas::util::InvalidParameterException);

  void sendInitString() throw(nidas::util::IOException);

  bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

  /**
   * Max # for array sizing.  Valid number of channels are 10, 20, 30 and 40.
   */
  static const int MAX_CHANNELS = 40;

  // Packet to initialize probe with.
  struct Init200_blk
  {
    ushort  id;                                 // ESC,id
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
  typedef struct Init200_blk Init200_blk;

  // Data packet back from probe.
  struct DMT200_blk
  {
      unsigned short cabinChan[8];
      unsigned short range;
      unsigned short fill;
      unsigned short AvgTransit;
      unsigned short FIFOfull;
      unsigned short resetFlag;
      unsigned short SyncErrA;
      unsigned short SyncErrB;
      unsigned short SyncErrC;
      unsigned long ADCoverflow;
      unsigned long OPCchan[MAX_CHANNELS];	// 40 channels max
  };
  typedef struct DMT200_blk DMT200_blk;

private:

  Init200_blk _setup_pkt;
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

};

}}}	// namespace nidas namespace dynld raf

#endif
