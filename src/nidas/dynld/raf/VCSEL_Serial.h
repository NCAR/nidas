/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/VCSEL_Serial.h $

 ******************************************************************
*/

#ifndef _nidas_dynld_raf_vcsel_serial_h_
#define _nidas_dynld_raf_vcsel_serial_h_

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/core/DerivedDataClient.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * VCSEL Serial Sensor.  This would be able to use the generic SerialSensor class
 * except for the need to send Ambient Temperature up to the instrument.
 */
class VCSEL_Serial : public DSMSerialSensor, public DerivedDataClient
{

public:
    VCSEL_Serial();
    ~VCSEL_Serial();

    /**
     * open the sensor and perform any intialization to the driver.
     */
    void open(int flags) throw(nidas::util::IOException);

    void close() throw(nidas::util::IOException);

    int getATXRate() const { return _atxRate; }

    void setATXRate(int val) { _atxRate = val; }
    
    virtual void
    derivedDataNotify(const nidas::core:: DerivedDataReader * s)
        throw();


protected:
    /**
     * Send the ambient temperature up to the VCSEL.
     * @param atx is the ambient temperature to send.
     */
    virtual void sendAmbientTemperature(float atx) throw(nidas::util::IOException);

    /**
     * How often to send ATX to the VCSEL.
     */
    int _atxRate;

};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
