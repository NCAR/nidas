/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: $

 ******************************************************************
*/
#ifndef IRS_HW_HG2001GD_H
#define IRS_HW_HG2001GD_H

#include <DSMArincSensor.h>

namespace dsm {

/**
 * ARINC IRS label processor.
 *
 * Taken from the Honeywell Installation Manual PN HG2021GB02/GD02
 * Table 207 "GNSSU ARINC 429 Output Data" (GPS)  (pages 217-219).
 */
class IRS_HW_HG2001GD : public DSMArincSensor {

public:

  /**
   * No arg constructor.  Typically the device name and other
   * attributes must be set before the sensor device is opened.
   */
  IRS_HW_HG2001GD() {err("");}
  //DSMArincSensor();
  //~DSMArincSensor();

  /** Process all labels from this instrument. */
  float processLabel(const unsigned long data)
    throw(atdUtil::IOException);
};

}

#endif
