/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_RAF_IRS_HW_YG1854_H
#define NIDAS_DYNLD_RAF_IRS_HW_YG1854_H

#include <nidas/dynld/raf/Arinc_IRS.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * ARINC IRS label processor.
 *
 * Taken from the Honeywell installation manual for the
 * YG1854 LASERREF SM IRS/GPIRS        (pages 640-650).
 */
class IRS_HW_YG1854 : public Arinc_IRS {

public:

  /**
   * No arg constructor.  Typically the device name and other
   * attributes must be set before the sensor device is opened.
   */
  IRS_HW_YG1854() {
#ifdef DEBUG
  	err("");
#endif
  }

  /** Process all labels from this instrument. */
  float processLabel(const long data);
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
