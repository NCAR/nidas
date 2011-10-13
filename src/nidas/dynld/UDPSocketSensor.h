/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_UDPSOCKETSENSOR_H_
#define NIDAS_DYNLD_UDPSOCKETSENSOR_H

#include <nidas/core/CharacterSensor.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * Build a generic UDP socket sensor reader. It get host broadcast ip and port from xml file, 
 * make a UDP socket connection, parse the input data into the format as configured in the xml file.
 *
 */
class UDPSocketSensor: public CharacterSensor
{

public:

    UDPSocketSensor();

    virtual ~UDPSocketSensor() { }

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

private:

};

}}	// namespace nidas namespace dynld

#endif
