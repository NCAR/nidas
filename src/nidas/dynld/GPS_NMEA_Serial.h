// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate$

   $LastChangedRevision$

   $LastChangedBy$

   $HeadURL$

*/

#ifndef NIDIS_DYNLD_GPS_NMEA_SERIAL_H
#define NIDIS_DYNLD_GPS_NMEA_SERIAL_H

#include <nidas/dynld/DSMSerialSensor.h>

namespace nidas { namespace dynld {

/**
 * A class for reading NMEA records from a GPS attached to
 * a serial port.
 */
class GPS_NMEA_Serial: public DSMSerialSensor
{
public:

    GPS_NMEA_Serial();

    ~GPS_NMEA_Serial();

    void addSampleTag(SampleTag* stag)
        throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

private:

    dsm_time_t parseGGA(const char* input,double *dout,int nvars,dsm_time_t tt) 
        throw();

    dsm_time_t parseRMC(const char* input,double *dout,int nvars,dsm_time_t tt)
        throw();

    /**
     * Number of variables requested from GGA record (sample id == 1)
     */
    int _ggaNvars;

    /**
     * Full sample id of GGA variables.
     */
    dsm_sample_id_t _ggaId;

    /**
     * Number of variables requested from RMC record (sample id == 2)
     */
    int _rmcNvars;

    /**
     * Full sample id of RMC variables.
     */
    dsm_sample_id_t _rmcId;

    /**
     * Id of sample from GGA NMEA record.  Fixed at 1.
     */
    static const int GGA_SAMPLE_ID;

    /**
     * Id of sample from RMC NMEA record.  Fixed at 2.
     */
    static const int RMC_SAMPLE_ID;

};

}}	// namespace nidas namespace dynld

#endif
