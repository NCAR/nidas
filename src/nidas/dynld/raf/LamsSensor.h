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


#ifndef NIDAS_DYNLD_LAMSSENSOR_H
#define NIDAS_DYNLD_LAMSSENSOR_H

// Linux include files.
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/select.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <bits/posix1_lim.h> // SSIZE_MAX
#include <signal.h>          // sigaction
#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/core/RTL_IODevice.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/rtlinux/lams.h>


namespace nidas { namespace dynld { namespace raf {
  using namespace std;
  using namespace nidas::core;
  namespace n_u = nidas::util;
 
class LamsSensor : public DSMSensor
{
public:

  bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

  IODevice* buildIODevice() throw(n_u::IOException);
  
  SampleScanner* buildSampleScanner()
  {
    return new SampleScanner();
  }
 
  /**
     * Open the device connected to the sensor.
     */
  void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    /**
     * Close the device connected to the sensor.
     */
  void close() throw(nidas::util::IOException);
};

}}}

#endif
