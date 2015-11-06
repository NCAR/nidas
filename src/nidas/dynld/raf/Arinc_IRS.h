// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
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
