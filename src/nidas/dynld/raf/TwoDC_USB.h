/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoDC_USB.h $

 ******************************************************************
*/

#ifndef _nidas_dynld_raf_2dc_usb_h_
#define _nidas_dynld_raf_2dc_usb_h_

#include <nidas/core/DSMSensor.h>
#include <nidas/core/DerivedDataClient.h>
#include <nidas/util/EndianConverter.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Sensors connected to the Mesa AnythingIO card.  Current programming is
 * for a PMS1D-260X, Pulse Counting, and the APN-232 Radar Altimeter.
 * Digital in/out coming soon.
 */
class TwoDC_USB : public DSMSensor, public DerivedDataClient {

public:
  TwoDC_USB();
  ~TwoDC_USB();

  IODevice* buildIODevice() throw(nidas::util::IOException);

  SampleScanner* buildSampleScanner();

  int getDefaultMode() const { return O_RDWR; }

  /**
   * open the sensor and perform any intialization to the driver.
   */
  virtual void
  open(int flags) throw(nidas::util::IOException);

  virtual void
  close() throw(nidas::util::IOException);

  virtual void
  fromDOMElement(const xercesc::DOMElement *)
    throw(nidas::util::InvalidParameterException);

  virtual bool
  process(const Sample * samp, std::list<const Sample *>& results)
	throw();

  virtual void
  derivedDataNotify(const nidas::core::DerivedDataReader * s) throw();


protected:

  // Probe produces Big Endian.
  static const nidas::util::EndianConverter * toLittle;

  /**
   * Encode and send the true airspeed to the USB driver, which will
   * in turn send it to the probe.
   */
  void sendTrueAirspeed(float tas);

  /**
   * Probe resolution in meters.  Acquired from XML config file.
   */
  double _resolution;

  /**
   * Synchword mask.  This slice/word is written at the end of each particle.
   * 28 bits of synchronization and 36 bits of timing information.
   */
  static const long long _syncMask, _syncWord;
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
