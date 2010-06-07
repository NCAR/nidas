/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_RAF_ADC_HW_EB7022597_H
#define NIDAS_DYNLD_RAF_ADC_HW_EB7022597_H

#include <nidas/dynld/raf/DSMArincSensor.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * ARINC ADC label processor.
 *
 * Taken from the Honeywell Engineering Specification A.4.1 spec. no.
 * EB7022597 cage code 55939 "Air Data Computer"    (pages A-53..79).
 */
class ADC_HW_EB7022597 : public DSMArincSensor {

public:

  /**
   * No arg constructor.  Typically the device name and other
   * attributes must be set before the sensor device is opened.
   */
  ADC_HW_EB7022597() {
#ifdef DEBUG
  	//err("");
#endif
  }

  /** Process all labels from this instrument. */
  float processLabel(const int data);
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
