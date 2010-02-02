/* -*- mode: C++; c-basic-offset: 4; -*-
    Copyright 2006 UCAR, NCAR, All Rights Reserved
*/

#ifndef NIDAS_DYNLD_ISS_WXTSENSOR_H
#define NIDAS_DYNLD_ISS_WXTSENSOR_H

#include <nidas/dynld/DSMSerialSensor.h>

namespace nidas { namespace dynld { namespace iss {

using namespace nidas::core;

using nidas::dynld::DSMSerialSensor;
using nidas::util::IOException;
using nidas::util::InvalidParameterException;

/**
 * Vaisala WXT weather sensor.
 *
 * This is a serial sensor, but the messages contain multiple fields, and
 * one or more of the fields may be missing values.  The missing value is
 * indicated by a '#' in place of the Units character specifier.  This
 * makes it infeasible to parse the message with the normal serial sensor
 * scanf string.  Instead, the scanf format string is comma-separated along
 * with the raw data message, and variables are scanned individually.
 */
class WxtSensor: public DSMSerialSensor
{

public:

    WxtSensor();

    ~WxtSensor();

    int
    scanSample(AsciiSscanf* sscanf, const char* inputstr, float* data_ptr);

    void
    fromDOMElement(const xercesc::DOMElement* node)
	throw(InvalidParameterException);

};

}}}	// namespace nidas namespace dynld namespace iss

#endif // NIDAS_DYNLD_ISS_WXTSENSOR_H
