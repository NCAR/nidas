// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate$

   $LastChangedRevision$

   $LastChangedBy$

   $HeadURL$

*/

#ifndef NIDIS_DYNLD_GPS_NMEA_NETSENSOR_H
#define NIDIS_DYNLD_GPS_NMEA_NETSENSOR_H

#include <nidas/dynld/UDPSocketSensor.h>
#include <nidas/dynld/GPS_NMEA_Process.h>

namespace nidas { namespace dynld {

/**
 * A class for reading NMEA records from a GPS attached to
 * a network port.  The process() method parses GPGGA and GPRMC
 * NMEA messages and generates floating point samples.
 */
class GPS_NMEA_NetSensor: public UDPSocketSensor
{
public:

    class My_GPS_NMEA_Process: public GPS_NMEA_Process {
    public:
        My_GPS_NMEA_Process(DSMSensor* dS):GPS_NMEA_Process(dS) {};
    } _processor;

    GPS_NMEA_NetSensor();

    void addSampleTag(SampleTag* stag)
        throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

    SampleScanner* buildSampleScanner()
    	throw(nidas::util::InvalidParameterException);
};

}}	// namespace nidas namespace dynld

#endif
