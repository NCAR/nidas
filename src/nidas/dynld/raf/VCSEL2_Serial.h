// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision$

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

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
class VCSEL2_Serial : public DSMSerialSensor, public DerivedDataClient
{

public:
    VCSEL2_Serial();
    ~VCSEL2_Serial();

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
     * @param psx is the static pressure to send.
     */
    virtual void sendTemperaturePressure(float atx, float psx) throw(nidas::util::IOException);

    /**
     * How often to send ATX to the VCSEL.
     */
    int _atxRate;

};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
