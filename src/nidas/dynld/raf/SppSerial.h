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
 * Class for unsigned 2-byte data from Droplet Measurement Technologies
 * probes.  The DMT byte order is 01, where byte 0 is the low-order byte.
 *
 * Data are stored within the class in the order sent by the DMT probe, so
 * data from the DMT can be memcpy-ed into this class and used directly.
 * This class contains only byte storage, so alignment should not be an
 * issue if this is part of a struct (i.e., a DMT data packet can be
 * memcpy-ed directly into a struct built of appropriate DMT_* elements,
 * and each element should work properly regardless of its specific byte
 * alignment within the struct).
 */
class DMT_UShort
{
public:
  DMT_UShort() {
    putValue(0);
  }

  DMT_UShort(unsigned short val) {
    putValue(val);
  }

  /**
   * Return the DMT value as a local unsigned short
   */
  inline unsigned short value() const { 
    unsigned short val = bytes[1] << 8 | bytes[0];
    return val;
  }

  /**
   * Pack a local unsigned short into DMT order
   */
  inline void putValue(unsigned short val) {
    bytes[0] = val & 0xff;        // 0
    bytes[1] = (val >> 8) & 0xff; // 1
  }
  
  unsigned char bytes[2];
};

/**
 * Class for unsigned 4-byte data from Droplet Measurement Technologies
 * probes.  The DMT byte order is 2301, where byte 0 is the low-order byte.
 *
 * Data are stored within the class in the order sent by the DMT probe, so
 * data from the DMT can be memcpy-ed into this class and used directly.
 * This class contains only byte storage, so alignment should not be an
 * issue if this is part of a struct (i.e., a DMT data packet can be
 * memcpy-ed directly into a struct built of appropriate DMT_* elements,
 * and each element should work properly regardless of its specific byte
 * alignment within the struct).
 */
class DMT_ULong
{
public:
  DMT_ULong() {
    putValue(0);
  }

  DMT_ULong(unsigned long val) {
    putValue(val);
  }
  
  /**
   * Return the DMT value as a local unsigned long
   */
  inline unsigned long value() const { 
    unsigned long val = bytes[1] << 24 | bytes[0] << 16 | 
      bytes[3] << 8 | bytes[2];
    return val;
  }
  
  /**
   * Pack a local unsigned long into DMT order
   */
  inline void putValue(unsigned long val) {
    bytes[0] = (val >> 16) & 0xff; // 2
    bytes[1] = (val >> 24) & 0xff; // 3
    bytes[2] = val & 0xff;         // 0
    bytes[3] = (val >> 8) & 0xff;  // 1
  }
  
  unsigned char bytes[4];
};


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

  SppSerial();

  unsigned short computeCheckSum(const unsigned char *pkt, int len);

  void fromDOMElement(const xercesc::DOMElement* node)
    throw(nidas::util::InvalidParameterException);

  /**
   * Max # for array sizing.  Valid number of channels are 10, 20, 30 and 40.
   */
  static const int MAX_CHANNELS = 40;


protected:
  /**
   * Return the expected packet length in bytes given the number of channels
   * being used.
   */
  virtual int calculatePacketLen(int nchannels) const = 0;

  /**
   * Append _packetLen bytes of data to _waitingData, and find the earliest
   * "good" record possible, where a good record:
   * 
   *  1) is _packetLen bytes long
   *  2) the last two bytes of the record are a valid 16-bit checksum 
   *     for the rest of the record (_dataType == FixedLength), or the 
   *	 last two bytes match the expected record terminator 
   *     (_dataType == Delimited)
   *
   * If a good record is found, the function returns true, data before the 
   * good record are dropped, leaving the good record will be at the head of 
   * _waitingData.  If no good record is found, the function returns false,
   * and the last (_packetLen-1) bytes of _waitingData are retained.
   */
  int appendDataAndFindGood(const Sample* sample);

  unsigned short _model;

  /**
   * Number of channels requested to be recorded.
   */
  int _nChannels;

  /**
   * Number of housekeeping variables added to output data
   */
  int _nHskp;
  

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
   * length (_packetLen) so that we can hold the current incoming chunk
   * plus anything remaining from the previous chunk.  _nWaitingData keeps
   * track of how much of the buffer is actually in use.
   */
  unsigned char* _waitingData;  // size will be 2 * _packetLen
  unsigned short _nWaitingData;
  int _skippedBytes;		// how much skipped looking for a good record?

  /**
   * Here more for documentation.  This is the data polling request packet.
   */
  struct reqPckt
  {
    char esc;		// 0x1b
    char id;		// 0x02
    DMT_UShort cksum;	// 0x001d
  };

};

}}}	// namespace nidas namespace dynld raf

#endif
