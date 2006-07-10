/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved
*/

#ifndef NIDAS_CORE_RTL_DEVIOCTLSTORE_H
#define NIDAS_CORE_RTL_DEVIOCTLSTORE_H

#include <vector>

#include <nidas/core/RTL_DevIoctl.h>

#include <nidas/util/ThreadSupport.h>

namespace nidas { namespace core {

/**
 * Singleton class holding all our RTL_DevIoctls.
 * RTL_DevIoctls are shareable by multiple RTL_DSMSensors. The idea
 * is that one RTL_DevIoctl is needed for each interface board,
 * and one board may have multiple ports, with each port
 * corresponding to Sensor.  So this class is where one goes to
 * get references to the RTL_DevIoctl for an RTL_DSMSensor.
 */
class RTL_DevIoctlStore {
public:
    /**
     * Fetch the pointer to the instance of RTL_DevIoctlStore
     */
    static RTL_DevIoctlStore* getInstance();

    /**
     * Delete the instance of RTL_DevIoctlStore. Probably only done
     * at main program shutdown, and then only if you're really
     * worried about cleaning up.
     */
    static void removeInstance();

    RTL_DevIoctl* getDevIoctl(const std::string& prefix, int portNum)
    	throw(nidas::util::IOException);

private:

    /**
     * Constructor. Private. Create the instance of RTL_DevIoctlStore
     * using getInstance();
     */
    RTL_DevIoctlStore();
    ~RTL_DevIoctlStore();

    RTL_DevIoctl* getDevIoctl(const std::string& prefix, int boardNum,
    	int firstPort) throw(nidas::util::IOException);

    static RTL_DevIoctlStore* instance;

    std::vector<RTL_DevIoctl*> fifos;

    static nidas::util::Mutex instanceMutex;

    nidas::util::Mutex fifosMutex;
};

}}	// namespace nidas namespace core

#endif
