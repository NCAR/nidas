/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef GPS_HW_HG2021GB02_H
#define GPS_HW_HG2021GB02_H

#include <DSMArincSensor.h>

namespace dsm {

/**
 * ARINC IRS label processor.
 *
 * Taken from the Honeywell Installation Manual PN HG2021GB02/GD02
 * Table 207 "GNSSU ARINC 429 Output Data" (GPS)  (pages 217-219).
 */
class GPS_HW_HG2021GB02 : public DSMArincSensor {

public:

  /**
   * No arg constructor.  Typically the device name and other
   * attributes must be set before the sensor device is opened.
   */
  GPS_HW_HG2021GB02() {err("");}

  /** Process a raw sample, which in this case means create
   * a list of samples with each sample containing a timetag.
   * Note that the GPS needs to specialise its process method
   * to merge values that span more then one label. */
  virtual bool process(const Sample*, std::list<const Sample*>& result)
    throw();

  /** Process all labels from this instrument. */
  float processLabel(const unsigned long data);
};

}

#endif
