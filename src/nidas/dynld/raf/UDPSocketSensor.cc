/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2008-04-04 10:33:45 -0600 (Fri, 04 Apr 2008) $

    $LastChangedRevision: 4149 $

    $LastChangedBy: dongl $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/raf/UDPSocketSensor.cc $

*/

#include <nidas/dynld/raf/UDPSocketSensor.h>
#include <nidas/core/UDPSocketIODevice.h>
#include <nidas/core/DSMTime.h>

#include <nidas/util/Logger.h>

#include <sstream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,UDPSocketSensor)

UDPSocketSensor::UDPSocketSensor()
{
}

IODevice* UDPSocketSensor::buildIODevice() throw(n_u::IOException)
{
    UDPSocketIODevice* dev = new UDPSocketIODevice();
    return dev;
}

SampleScanner* UDPSocketSensor::buildSampleScanner()
{
    DatagramSampleScanner* scanner = new DatagramSampleScanner();
    scanner->setNullTerminate(doesAsciiSscanfs());
    return scanner;
}
