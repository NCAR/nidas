
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef NIDAS_CORE_SENSOROPENER_H
#define NIDAS_CORE_SENSOROPENER_H

#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/IOException.h>

namespace nidas { namespace core {

class SensorHandler;

/**
 * Thread which opens DSMSensors.
 */
class SensorOpener : public nidas::util::Thread {
public:

    /**
     * Constructor.
     * @param rserialPort TCP socket port to listen for incoming
     *		requests to the rserial service. 0=don't listen.
     */
    SensorOpener(SensorHandler*);
    ~SensorOpener();

    /**
     * A SensorHandler calls this method to schedule
     * a sensor to be opened.  SensorOpener will
     * call the SensorHandler::sensorOpen() method after
     * the sensor has been successfully opened, to notify
     * the PortSelect that the sensor is ready for IO.
     * The SensorHandler owns the DSMSensor pointer.
     * SensorHandler will cancel the SensorOpener thread
     * before deleting any DSMSensors.
     */
    void openSensor(DSMSensor *sensor);

    /**
     * A SensorHandler calls this method to schedule
     * a sensor to be reopened. This is typically done
     * on a sensor that has returned an IOException on a
     * read, in which case the SensorHandler closes the
     * sensor and requests a reopen.
     */
    void reopenSensor(DSMSensor *sensor);

    int run() throw(nidas::util::Exception);

    void interrupt();

protected:
    SensorHandler* selector;

    std::list<DSMSensor*> sensors;

    std::list<DSMSensor*> problemSensors;

    nidas::util::Cond sensorCond;
};

}}	// namespace nidas namespace core

#endif
