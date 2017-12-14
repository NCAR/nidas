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

#ifndef _nidas_dynld_raf_TwoD_House_h_
#define _nidas_dynld_raf_TwoD_House_h_

#include <nidas/dynld/DSMSerialSensor.h>

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
