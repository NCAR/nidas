// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
/* LamsSensor.h

*/


#ifndef NIDAS_DYNLD_RAF_LAMSSENSOR_H
#define NIDAS_DYNLD_RAF_LAMSSENSOR_H

#include <nidas/linux/lams/lamsx.h>

#include <nidas/core/DSMSensor.h>
#include <nidas/core/DerivedDataClient.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;
namespace n_u = nidas::util;
 
/**
 * Sensor class supporting the NCAR/EOL Laser Air Motion Sensor (LAMS)
 * via a DSM.  This is the original LAMS implementation.
 */
class LamsSensor : public DSMSensor, public DerivedDataClient
{
public:
  LamsSensor();

  void fromDOMElement(const xercesc::DOMElement* node)
      throw(nidas::util::InvalidParameterException);

  void printStatus(std::ostream& ostr) throw();

  virtual void
  derivedDataNotify(const nidas::core:: DerivedDataReader * s)
        throw();

  bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();
	
  IODevice* buildIODevice() throw(n_u::IOException);
  	
  SampleScanner* buildSampleScanner() throw(n_u::InvalidParameterException);
  
  /**
   * Open the device connected to the sensor.
   */
  void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

  void close() throw(nidas::util::IOException);

private:

  int nAVG;
  
  int nPEAK;

  float TAS_level;
  enum {BELOW, ABOVE} TASlvl;

  float tas;     // True Airspeed.  Meters per second
  int tas_step;  // generate a True Airspeed (set to 0 to disable)

  /**
   * Number of initial spectral values to skip when reading from the card.
   */
  int nSKIP;
};

}}}

#endif
