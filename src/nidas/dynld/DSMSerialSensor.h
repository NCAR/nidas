// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_DSMSERIALSENSOR_H
#define NIDAS_DYNLD_DSMSERIALSENSOR_H

#include <nidas/core/SerialSensor.h>
#include <nidas/core/LooperClient.h>
#include <nidas/util/Termios.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * A sensor connected to a serial port.
 */
class DSMSerialSensor : public SerialSensor
{
public:

};

}}	// namespace nidas namespace dynld

#endif
