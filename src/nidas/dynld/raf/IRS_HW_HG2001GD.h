/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_RAF_IRS_HW_HG2001GD_H
#define NIDAS_DYNLD_RAF_IRS_HW_HG2001GD_H

#include <nidas/dynld/raf/Arinc_IRS.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * ARINC IRS label processor.
 *
 * Taken from the Honeywell installation manual for the
 * Inertial Reference Unit / Part No. HG2001GD (pages 649-651).
 */
class IRS_HW_HG2001GD : public Arinc_IRS {

public:

  /**
   * No arg constructor.  Typically the device name and other
   * attributes must be set before the sensor device is opened.
   */
  IRS_HW_HG2001GD() {
#ifdef DEBUG
  	err("");
#endif
  }

  /** Process all labels from this instrument. */
  float processLabel(const int data);
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
