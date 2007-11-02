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
 *
 * Sample looks like (values in hex):
 *	0000 0000 0000 0000 0000 05 00e4
 *
 * First 5 are shadow-or counts and should be summed to produce 1 value.
 * The sixth value is the housekeeping index number and the seventh is
 * the housekeeping value.  There are 8 housekeeping values, you get one
 * each sample.  So we will collect them and report the same value for
 * 8 seconds.
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
  float _houseKeeping[8];

  static const size_t V15_INDX, TMP_INDX, EE1_INDX, EE32_INDX,
	VN15_INDX, V5_INDX;
};

}}}	// namespace nidas namespace dynld raf

#endif
