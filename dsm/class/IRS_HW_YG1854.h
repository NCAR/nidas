/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef IRS_HW_YG1854_H
#define IRS_HW_YG1854_H

#include <Arinc_IRS.h>

namespace dsm {

/**
 * ARINC IRS label processor.
 *
 * Taken from the Honeywell installation manual for the
 * YG1854 LASERREF SM IRS/GPIRS        (pages 648-650).
 */
class IRS_HW_YG1854 : public Arinc_IRS {

public:

  /**
   * No arg constructor.  Typically the device name and other
   * attributes must be set before the sensor device is opened.
   */
  IRS_HW_YG1854() {err("");}

  /** Process all labels from this instrument. */
  float processLabel(const unsigned long data);
};

}

#endif
