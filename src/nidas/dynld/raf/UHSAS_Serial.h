/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/UHSAS_Serial.h $

*/

#ifndef NIDAS_DYNLD_RAF_UHSAS_SERIAL_H
#define NIDAS_DYNLD_RAF_UHSAS_SERIAL_H

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/util/EndianConverter.h>

#include <iostream>

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading the UHSAS probe.  This appears to be an updated PCASP.
 * RS-232 @ 115,200 baud.
 */
class UHSAS_Serial : public DSMSerialSensor
{
public:

  UHSAS_Serial() : DSMSerialSensor() {}

  void fromDOMElement(const xercesc::DOMElement* node)
      throw(nidas::util::InvalidParameterException);

  void sendInitString() throw(nidas::util::IOException);

  bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

  void addSampleTag(SampleTag* tag)
        throw(nidas::util::InvalidParameterException);


protected:

  static const nidas::util::EndianConverter * toLittle;

  /**
   * Total number of floats in the processed output sample.
   */
  int _noutValues;

  /**
   * Number of channels requested to be recorded.  Fixed at 101 for
   * this probe.
   */
  int _nChannels;

  /**
   * Number of housekeeping channels.  12 of 16 possible are used.
   */
  int _nHousekeep;

  /**
   * Stash sample-rate.  The rw histogram counts we want to convert to
   * a counts per second by multiplying by sample rate.
   */
  unsigned int _sampleRate;

};

}}}	// namespace nidas namespace dynld raf

#endif
