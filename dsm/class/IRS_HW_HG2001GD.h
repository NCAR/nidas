/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef IRS_HW_HG2001GD_H
#define IRS_HW_HG2001GD_H

#include <Arinc_IRS.h>

namespace dsm {

/**
 * ARINC IRS label processor.
 *
 * Taken from the Honeywell Installation Manual PN HG2021GB02/GD02
 * Table 207 "GNSSU ARINC 429 Output Data" (GPS)  (pages 217-219).
 */
class IRS_HW_HG2001GD : public Arinc_IRS {

public:

  /**
   * No arg constructor.  Typically the device name and other
   * attributes must be set before the sensor device is opened.
   */
  IRS_HW_HG2001GD() {err("");}

  /** Process all labels from this instrument. */
  float processLabel(const unsigned long data);
};

}

#endif
