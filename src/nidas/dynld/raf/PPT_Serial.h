// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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

#ifndef _nidas_dynld_raf_ppt_serial_h_
#define _nidas_dynld_raf_ppt_serial_h_

#define PROMPT_PREFIX "*00"
#define PARITY_ERROR "#00CP!0.0000"
#define BUFFER_ERROR "*9dP1!"

#include <nidas/core/SerialSensor.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Honeywell PTT Pressure Transducer Serial Sensor.  
 * This would be able to use the generic SerialSensor class
 * except for the fact that negative pressures (differential) can 
 * have a space between the minus sign and the numeric value and 
 * sscanf doesn't have a form for that, and that we'd like
 * to get temperature at 1 Hz and Pressure at 100 Hz.
 */
class PPT_Serial : public SerialSensor
{

public:
    PPT_Serial();
    ~PPT_Serial();

    /**
     * open the sensor and perform any intialization to the driver.
     */
    void open(int flags) throw(nidas::util::IOException);

    void close() throw(nidas::util::IOException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();


protected:

private:
    int _numPromptsBack;
    int _numParityErr;
    int _numBuffErr;
};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
