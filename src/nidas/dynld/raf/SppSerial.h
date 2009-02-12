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

//
// Add a bogus zeroth bin to the data to match historical behavior.
// Remove all traces of this after the netCDF file refactor.
//
#define ZERO_BIN_HACK


namespace nidas { namespace dynld { namespace raf {

/**
 * DMT 2-byte ints are packed with byte order 01, where byte 0 is the
 * low-order byte.
 *
 * DMT_UShort is an opaque 2-byte value (just a 2-byte unsigned char array)
 * in DMT order.
 *
 * UnpackDMT_UShort unpacks a DMT_UShort as a local unsigned short.
 *
 * PackDMT_UShort packs a local unsigned short into a DMT_UShort.
 */
typedef unsigned char DMT_UShort[2];

inline unsigned short UnpackDMT_UShort(DMT_UShort dmtval)
{
  unsigned short val = (dmtval[1] << 8) | dmtval[0];
  return val;
}

inline void PackDMT_UShort(DMT_UShort dmtval, unsigned short val) 
{
  dmtval[0] = val & 0xff;
  dmtval[1] = (val >> 8) & 0xff;
}


/**
 * DMT 4-byte ints are packed with byte order 2301, where byte 0 is the
 * low-order byte.
 *
 * DMT_ULong is an opaque 4-byte value (just a 4-byte unsigned char array)
 * in DMT order.
 *
 * UnpackDMT_ULong unpacks a DMT_ULong as a local unsigned long.
 *
 * PackDMT_ULong packs a local unsigned long into a DMT_ULong.
 */
typedef unsigned char DMT_ULong[4];

inline unsigned long UnpackDMT_ULong(DMT_ULong dmtval)
{
  unsigned long val = dmtval[1] << 24 | dmtval[0] << 16 | 
    dmtval[3] << 8 | dmtval[2]; // DMT byte order is 2301
  return val;
}
      
inline void PackDMT_ULong(DMT_ULong dmtval, unsigned long val) 
{
  dmtval[0] = (val >> 16) & 0xff; // 2
  dmtval[1] = (val >> 24) & 0xff; // 3
  dmtval[2] = val & 0xff;         // 0
  dmtval[3] = (val >> 8) & 0xff;  // 1
}


/**
 * Base class for many DMT Probes, including SPP100, SPP200, SPP300 and the CDP.
 */
class SppSerial : public DSMSerialSensor
{
public:
  enum DataTermination
  {
    FixedLength,	// CheckSum
    Delimited
  };

  SppSerial(const std::string & probe);
  ~SppSerial();

  unsigned short computeCheckSum(const unsigned char *pkt, int len);

  void validate()
    throw(nidas::util::InvalidParameterException);

  void fromDOMElement(const xercesc::DOMElement* node)
    throw(nidas::util::InvalidParameterException);

  /**
   * Max # for array sizing.  Valid number of channels are 10, 20, 30 and 40.
   */
  static const int MAX_CHANNELS = 40;


protected:
  /**
   * Used to acquire sample rate to convert data to counts per second.
   */
  void addSampleTag(SampleTag* tag)
        throw(nidas::util::InvalidParameterException);

  /**
   * Return the expected data packet length in bytes based on the number of
   * channels being used.
   */
  virtual int packetLen() const = 0;

  /**
   * Send pre-packaged initialization packet to SPP probe and what for
   * Acknowledge packet.
   */
  virtual void
  sendInitPacketAndCheckAck(void * packet, int len);

  /**
   * Append _packetLen bytes of data to _waitingData, and find the earliest
   * "good" record possible, where a good record:
   * 
   *  1) is packetLen() bytes long
   *  2) the last two bytes of the record are a valid 16-bit checksum 
   *     for the rest of the record (_dataType == FixedLength), or the 
   *	 last two bytes match the expected record terminator 
   *     (_dataType == Delimited)
   *
   * If a good record is found, the function returns true, data before the 
   * good record are dropped, leaving the good record will be at the head of 
   * _waitingData.  If no good record is found, the function returns false,
   * and the last (packetLen()-1) bytes of _waitingData are retained.
   */
  int appendDataAndFindGood(const Sample* sample);

  /// Possibly not needed...
  unsigned short _model;

  /**
   * Number of channels requested to be recorded.
   */
  int _nChannels;

  /**
   * Number of housekeeping variables added to output data
   */
  int _nHskp;
  
  std::string _probeName;

  unsigned short _range;

  unsigned short _triggerThreshold;

  unsigned short _avgTransitWeight;

  unsigned short _divFlag;

  unsigned short _opcThreshold[MAX_CHANNELS];

  /**
   * Total number of floats in the processed output sample.
   */
  int _noutValues;

  /**
   * Whether we are using fixed length data with checkSum (true),
   * or the modified chips with message terminators.  Default is FixedLength.
   * @see DataTermination
   */
  DataTermination _dataType;
  unsigned short _recDelimiter;  // only used if _dataType == Delimited

  size_t _checkSumErrorCnt;

  /**
   * Buffer to hold incoming data until we find a chunk that looks like a
   * valid DMT100 data packet.  The buffer size is 2 * the expected packet
   * length (packetLen()) so that we can hold the current incoming chunk
   * plus anything remaining from the previous chunk.  _nWaitingData keeps
   * track of how much of the buffer is actually in use.
   */
  unsigned char* _waitingData;  // size will be 2 * packetLen()
  unsigned short _nWaitingData;
  int _skippedBytes;		// how much skipped looking for a good record?
  size_t _skippedRecordCount;
  size_t _totalRecordCount;

  /**
   * Here more for documentation.  This is the data polling request packet.
   */
  struct reqPckt
  {
    char esc;		// 0x1b
    char id;		// 0x02
    DMT_UShort cksum;	// 0x001d
  };

  /**
   * Stash sample-rate.  The rw histogram counts we want to convert to
   * a counts per second by multiplying by sample rate.
   */
  unsigned int _sampleRate;

//@{
  /**
   * Whether to output DELTAT variable.  validate() will detect if DELTAT
   * variable is last variable in sample list of variables.  If so this is
   * set to true.
   */
  unsigned int _outputDeltaT;
  /**
   * Store previous time tag.  We can then generate a delta T for statistics or output.
   */
  int _prevTime;
//@}

};

}}}	// namespace nidas namespace dynld raf

#endif
