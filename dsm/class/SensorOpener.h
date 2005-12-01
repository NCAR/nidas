
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-11-20 08:58:12 -0700 (Sun, 20 Nov 2005) $

    $LastChangedRevision: 3133 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/PortSelector.h $
 ********************************************************************

*/


#ifndef DSM_SENSOROPENER_H
#define DSM_SENSOROPENER_H

#include <atdUtil/Thread.h>
#include <atdUtil/ThreadSupport.h>
#include <atdUtil/IOException.h>

namespace dsm {

class PortSelector;

/**
 * Thread which opens DSMSensors.
 */
class SensorOpener : public atdUtil::Thread {
public:

    /**
     * Constructor.
     * @param rserialPort TCP socket port to listen for incoming
     *		requests to the rserial service. 0=don't listen.
     */
    SensorOpener(PortSelector*);
    ~SensorOpener();

    /**
     * A PortSelector calls this method to schedule
     * a sensor to be opened.  SensorOpener will
     * call the PortSelector::sensorOpen() method after
     * the sensor has been successfully opened, to notify
     * the PortSelect that the sensor is ready for IO.
     * The PortSelector owns the DSMSensor pointer.
     * PortSelector will cancel the SensorOpener thread
     * before deleting any DSMSensors.
     */
    void openSensor(DSMSensor *sensor);

    /**
     * A PortSelector calls this method to schedule
     * a sensor to be reopened. This is typically done
     * on a sensor that has returned an IOException on a
     * read, in which case the PortSelector closes the
     * sensor and requests a reopen.
     */
    void reopenSensor(DSMSensor *sensor);

    int run() throw(atdUtil::Exception);


protected:
    PortSelector* selector;

    std::list<DSMSensor*> sensors;

    std::list<DSMSensor*> problemSensors;

    atdUtil::Cond sensorCond;
};
}
#endif
