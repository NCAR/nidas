/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef DSMSERIALSENSOR_H
#define DSMSERIALSENSOR_H

#include <RTL_DSMSensor.h>
#include <atdTermio/Termios.h>

/**
 * A sensor connected to a serial port.
 */
class DSMSerialSensor : public RTL_DSMSensor, public atdTermio::Termios {

public:

    DSMSerialSensor(const std::string& name);
    ~DSMSerialSensor();

    /**
     * open the sensor. This opens the associated RT-Linux FIFOs.
     */
    void open(int flags) throw(atdUtil::IOException);

    void close() throw(atdUtil::IOException);

};

#endif
