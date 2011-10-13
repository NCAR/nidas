/* LamsNetSensor.h

   Copyright 2007 UCAR, NCAR, All Rights Reserved
   Revisions:
    $LastChangedRevision: $
    $LastChangedDate:  $
    $LastChangedBy:  $
    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynid/LamsNetSensor.h $
*/


#ifndef NIDAS_DYNLD_RAF_LAMSNETSENSOR_H
#define NIDAS_DYNLD_RAF_LAMSNETSENSOR_H

#include <iostream>
#include <iomanip>

#include <nidas/dynld/UDPSocketSensor.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;
namespace n_u = nidas::util;
 
/**
 * Sensor class supporting the NCAR/EOL Laser Air Motion Sensor (LAMS 3-beam)
 * via Ethernet UDP connection.
 */
class LamsNetSensor : public UDPSocketSensor
{
public:
  LamsNetSensor();

  bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();
	
private:

  static const int nBeams = 3;
  static const int LAMS_SPECTRA_SIZE = 512;

  const Sample *saveSamps[nBeams];
  int recordNumber[nBeams];
  int prevRecordNumber[nBeams];
};

}}}

#endif
