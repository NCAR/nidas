/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef DSM_A2DBOARDTEMPSENSOR_H
#define DSM_A2DBOARDTEMPSENSOR_H

#include <RTL_DSMSensor.h>
#include <irigclock.h>

namespace dsm {
/**
 * The I2C temperature sensor on the ADS3 A2D board.
 */
class A2DBoardTempSensor : public RTL_DSMSensor {

public:

    A2DBoardTempSensor();
    ~A2DBoardTempSensor();

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(atdUtil::IOException);

    void init() throw();
                                                                                
    /*
     * Close the device connected to the sensor.
     */
    void close() throw(atdUtil::IOException);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Process a raw sample, which in this case means convert
     * the signed short value to a temperature in celsius
     * by dividing by 16.0 degC-1
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

protected:
    enum irigClockRates rate;

    dsm_sample_id_t sampleId;

};

}

#endif
