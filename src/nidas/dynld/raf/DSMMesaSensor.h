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
  bool
  sendFPGACodeToDriver() throw(nidas::util::IOException);

  /**
   * Read the specified number of bytes from the specified offset in
   * the specified open file.
   *
   * Note: The file position is not restored to its original value.
   * Don't try to read 0 bytes.
   *
   * @returns The number of bytes read.
   *
   * @see sendFPGACodeToDriver()
   */
  size_t
  readbytesfromfile(FILE * f, long fromoffset, size_t numbytes,
		unsigned char * bufptr);

  /**
   * Set up for processing the input file.
   *
   * @see sendFPGACodeToDriver()
   */
  void
  selectfiletype(FILE * fp);

  long
  filelengthq(FILE * f);

  struct radar_set radar_info;
  struct pms260x_set p260x_info;
  struct counters_set counter_info;
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
