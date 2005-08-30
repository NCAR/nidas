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
    void open(int flags) throw(atdUtil::IOException,atdUtil::InvalidParameterException);

    void init() throw(atdUtil::InvalidParameterException);
                                                                                
    /*
     * Close the device connected to the sensor.
     */
    void close() throw(atdUtil::IOException);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Process a raw sample, which in this case means convert
     * the 16 bit signed value to degC.
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

protected:

    dsm_sample_id_t sampleId;

    /**
     * Sample rate.
     */
    enum irigClockRates rate;

    /**
     * Last measured temperature.  Provided in printStatus.
     */
    float lastTemp;

    /**
     * Conversion factor from 16 bit raw sensor value to degC
     */
    const float DEGC_PER_CNT;

};

}

#endif
