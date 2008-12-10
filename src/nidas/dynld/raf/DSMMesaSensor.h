/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision$

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#ifndef NIDAS_DYNLD_RAF_DSMMESASENSOR_H
#define NIDAS_DYNLD_RAF_DSMMESASENSOR_H

#include <nidas/linux/mesa.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Sensors connected to the Mesa AnythingIO card.  Current programming is
 * for a PMS1D-260X, Pulse Counting, and the APN-232 Radar Altimeter.
 * Digital in/out coming soon.
 */
class DSMMesaSensor : public DSMSensor {

public:
  DSMMesaSensor();
  ~DSMMesaSensor();

  bool isRTLinux() const;

  IODevice *
  buildIODevice() throw(nidas::util::IOException);

  SampleScanner* buildSampleScanner();

  /**
   * open the sensor and perform any intialization to the driver.
   */
  void
  open(int flags) throw(nidas::util::IOException,
    nidas::util::InvalidParameterException);

  void
  fromDOMElement(const xercesc::DOMElement *)
    throw(nidas::util::InvalidParameterException);

  bool
  process(const Sample * samp, std::list<const Sample *>& results)
	throw();

private:
  /**
   * Download FPGA code from flash/disk to driver.
   *
   * @returns whether file was succesfully transmitted.
   */
  void
  sendFPGACodeToDriver() throw(nidas::util::IOException);

  /**
   * Set up for processing the input file.
   *
   * @see sendFPGACodeToDriver()
   */
  void
  selectfiletype(FILE * fp,const std::string& fname) throw(nidas::util::IOException);

  struct radar_set radar_info;
  struct pms260x_set p260x_info;
  struct counters_set counter_info;

  mutable int _rtlinux;
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
