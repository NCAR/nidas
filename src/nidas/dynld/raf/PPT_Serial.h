/*
 ******************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/PPT_Serial.h $

 ******************************************************************
*/

#ifndef _nidas_dynld_raf_ppt_serial_h_
#define _nidas_dynld_raf_ppt_serial_h_

#include <nidas/dynld/DSMSerialSensor.h>

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
class PPT_Serial : public DSMSerialSensor
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

};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
