/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: $

 ******************************************************************
*/
#ifndef ARINC_IRS_H
#define ARINC_IRS_H

#include <DSMArincSensor.h>

namespace dsm {

/**
 * An Inertial Reference Sensor (IRS) connected to an ARINC port.
 */
class Arinc_IRS : public DSMArincSensor {

public:

  /**
   * No arg constructor.  Typically the device name and other
   * attributes must be set before the sensor device is opened.
   */
  Arinc_IRS();

  /** Extract the ARINC configuration elements from the XML header. */
  void fromDOMElement(const xercesc::DOMElement*)
    throw(atdUtil::InvalidParameterException);

 protected:

  /**
   * These are physical angular measurement offsets that describe
   * the orientation at which the IRS device is mounted on the aircraft.
   */
  float _irs_thdg_corr;
  float _irs_ptch_corr;
  float _irs_roll_corr;
};

}

#endif
