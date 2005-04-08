/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: $

 ******************************************************************
*/
#ifndef IRS_HW_YG1854_H
#define IRS_HW_YG1854_H

#include <DSMArincSensor.h>

namespace dsm {

/**
 * ARINC IRS label processor.
 *
 * Taken from the Honeywell installation manual for the
 * YG1854 LASERREF SM IRS/GPIRS        (pages 648-650).
 */
class IRS_HW_YG1854 : public DSMArincSensor {

public:

  /**
   * No arg constructor.  Typically the device name and other
   * attributes must be set before the sensor device is opened.
   */
  IRS_HW_YG1854() {err("");}
  //DSMArincSensor();
  //~DSMArincSensor();

  /** Process all labels from this instrument. */
  float processLabel(const unsigned long data)
    throw(atdUtil::IOException);
};

}

#endif
