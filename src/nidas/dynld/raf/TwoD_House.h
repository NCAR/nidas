/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoD_House.h $

*/

#ifndef _nidas_dynld_raf_TwoD_House_h_
#define _nidas_dynld_raf_TwoD_House_h_

#include <nidas/dynld/DSMSerialSensor.h>

#include <iostream>

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading and parsing the PMS2D Housekeeping (from the old
 * interface, not the new Fast 2DC).  RS-232 @ 9600 baud.  1Hz.
 */
class TwoD_House : public DSMSerialSensor
{
public:
  TwoD_House();
  ~TwoD_House();

  bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();


protected:

  /**
   * Total number of floats in the processed output sample.
   */
  int _noutValues;

  /**
   * Housekeeping.
   */
  float _houseKeeping[10];

};

}}}	// namespace nidas namespace dynld raf

#endif
