// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate$

   $LastChangedRevision$

   $LastChangedBy$

   $HeadURL$

*/

#include <nidas/dynld/GPS_NMEA_Serial.h>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(GPS_NMEA_Serial)

GPS_NMEA_Serial::GPS_NMEA_Serial():_processor(this)
{
}

void GPS_NMEA_Serial::addSampleTag(SampleTag* stag)
  throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::addSampleTag(stag);
    _processor.addSampleTag(stag);
}

bool GPS_NMEA_Serial::process(const Sample* samp,list<const Sample*>& results)
  throw()
{
    return _processor.process(samp, results);
}
