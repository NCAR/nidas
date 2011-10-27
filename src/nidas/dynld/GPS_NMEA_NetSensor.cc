// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate$

   $LastChangedRevision$

   $LastChangedBy$

   $HeadURL$

*/

#include <nidas/dynld/GPS_NMEA_NetSensor.h>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(GPS_NMEA_NetSensor)

GPS_NMEA_NetSensor::GPS_NMEA_NetSensor():_processor(this)
{
}

void GPS_NMEA_NetSensor::addSampleTag(SampleTag* stag)
  throw(n_u::InvalidParameterException)
{
    UDPSocketSensor::addSampleTag(stag);
    _processor.addSampleTag(stag);
}

bool GPS_NMEA_NetSensor::process(const Sample* samp,list<const Sample*>& results)
  throw()
{
    return _processor.process(samp, results);
}

SampleScanner* GPS_NMEA_NetSensor::buildSampleScanner()
  throw(n_u::InvalidParameterException)
{
    MessageStreamScanner*  scanner = new MessageStreamScanner();
    scanner->setNullTerminate(doesAsciiSscanfs());
    scanner->setMessageParameters(getMessageLength(),
        getMessageSeparator(),getMessageSeparatorAtEOM());
    return scanner;
}
