/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/raf/UDPSocketSensor.h $

*/

#ifndef NIDAS_DYNLD_RAF_UDPSOCKETSENSOR_H_
#define NIDAS_DYNLD_RAF_UDPSOCKETSENSOR_H

#include <nidas/dynld/UDPSocketSensor.h>

namespace nidas { namespace dynld { namespace raf {

/**
 * nidas::dynld::raf::UPDSocketSensor is the same as a nidas::dynld::UDPSocketSensor,
 * but kept around for legacy reasons - it's in quite a few XMLs.
 * At some point we might define a fromDOMElement() method here that gives a
 * "deprecated" message, indicating that one can use nidas::dynld::UDPSocketSensor.
 */
class UDPSocketSensor: public nidas::dynld::UDPSocketSensor
{

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
