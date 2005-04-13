/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: $

 ******************************************************************
*/
#ifndef ADC_HW_EB7022597_H
#define ADC_HW_EB7022597_H

#include <DSMArincSensor.h>

namespace dsm {

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
  ADC_HW_EB7022597() {err("");}

  /** Process all labels from this instrument. */
  float processLabel(const unsigned long data)
    throw(atdUtil::IOException);
};

}

#endif
