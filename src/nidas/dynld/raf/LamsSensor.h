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

// Linux include files.
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/select.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <bits/posix1_lim.h> // SSIZE_MAX
#include <signal.h>          // sigaction
#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/core/RTL_IODevice.h>
#include <nidas/rtlinux/lams.h>

#include <nidas/core/DSMSensor.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;
namespace n_u = nidas::util;
 
class LamsSensor : public DSMSensor
{
public:
  LamsSensor();
  ~LamsSensor();

  bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();
	
  IODevice* buildIODevice() throw(n_u::IOException)
  {
    return new RTL_IODevice();
  }
  	
  SampleScanner* buildSampleScanner()
  {
    return new DriverSampleScanner();
  }
 
  
  /**
   * Open the device connected to the sensor.
   */
  void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

private:
  struct lams_set lams_info;
};

}}}

#endif
