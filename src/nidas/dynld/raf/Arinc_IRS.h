/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_RAF_ARINC_IRS_H
#define NIDAS_DYNLD_RAF_ARINC_IRS_H

#include <nidas/dynld/raf/DSMArincSensor.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

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
  /// example XML:
  ///  <arincSensor ...
  ///      irs_thdg_corr="0.1" irs_ptch_corr="0.2" irs_roll_corr="0.3" />
  void fromDOMElement(const xercesc::DOMElement*)
    throw(nidas::util::InvalidParameterException);

 protected:

  /**
   * These are physical angular measurement offsets that describe
   * the orientation at which the IRS device is mounted on the aircraft.
   */
  float _irs_thdg_corr;
  float _irs_ptch_corr;
  float _irs_roll_corr;
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
