/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/raf/UDPSocketSensor.h $

*/

#ifndef NIDAS_DYNLD_RAF_UDPSOCKETSENSOR_H_
#define NIDAS_DYNLD_RAF_UDPSOCKETSENSOR_H

#include <nidas/core/CharacterSensor.h>

namespace nidas { namespace dynld { namespace raf {

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

    ~UDPSocketSensor() { }

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

private:

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
