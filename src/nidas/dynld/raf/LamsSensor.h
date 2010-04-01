/* LamsSensor.h

   This program is to open/init/close a sensor, 
   read data, process raw data to get wind speed, 
   and handle xml value for AEROS

   Copyright 2007 UCAR, NCAR, All Rights Reserved
   Revisions:
    $LastChangedRevision: $
    $LastChangedDate:  $
    $LastChangedBy:  $
    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynid/LamsSensor.h $
*/


#ifndef NIDAS_DYNLD_RAF_LAMSSENSOR_H
#define NIDAS_DYNLD_RAF_LAMSSENSOR_H

#include <iostream>
#include <iomanip>

// #include <nidas/linux/lams/lamsx.h>
#include <nidas/rtlinux/lams.h>

#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/core/RTL_IODevice.h>

#include <nidas/core/DSMSensor.h>
#include <nidas/core/DerivedDataClient.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;
namespace n_u = nidas::util;
 
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
  unsigned int nAVG;
  unsigned int nPEAK;

  float TAS_level;
  enum {BELOW, ABOVE} TASlvl;

  float tas;     // True Airspeed.  Meters per second
  int tas_step;  // generate a True Airspeed (set to 0 to disable)
};

}}}

#endif
