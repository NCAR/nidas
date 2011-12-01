// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/UDPSocketSensor.h>
#include <nidas/core/UDPSocketIODevice.h>

#include <nidas/util/Logger.h>

#include <sstream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(UDPSocketSensor)

UDPSocketSensor::UDPSocketSensor()
{
}

IODevice* UDPSocketSensor::buildIODevice() throw(n_u::IOException)
{
    UDPSocketIODevice* dev = new UDPSocketIODevice();
    return dev;
}

SampleScanner* UDPSocketSensor::buildSampleScanner()
    throw(n_u::InvalidParameterException)
{
    DatagramSampleScanner* scanner = new DatagramSampleScanner();
    scanner->setNullTerminate(doesAsciiSscanfs());
    return scanner;
}
