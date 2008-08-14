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
    
    GPS_NMEA_Serial():DSMSerialSensor() {
    }

    ~GPS_NMEA_Serial() {
       if (ggacnt > 0)
           std::cerr<<"\nTotal GPS_NMEA GGA time stamp errors: "<<ggacnt<<std::endl;
       if (rmccnt > 0)
           std::cerr<<"\nTotal GPS_NMEA RMC time stamp errors: "<<rmccnt<< std::endl; 
    }

    void addSampleTag(SampleTag* stag)
            throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    static void parseGGA(const char* input,float *dout,int nvars) 
    	throw();
  
    static void parseRMC(const char* input,float *dout,int nvars)
    	throw();

private:
    /**
     * Number of variables requested from GGA record (sample id == 1)
     */
    int ggaNvars;

    /**
     * Full sample id of GGA variables.
     */
    dsm_sample_id_t ggaId;

    /**
     * Number of variables requested from RMC record (sample id == 2)
     */
    int rmcNvars;

    /**
     * Full sample id of RMC variables.
     */
    dsm_sample_id_t rmcId;

    /**
     * Id of sample from GGA NMEA record.  Fixed at 1.
     */
    static const int GGA_SAMPLE_ID;

    /**
     * Id of sample from RMC NMEA record.  Fixed at 2.
     */
    static const int RMC_SAMPLE_ID;

    /**
     * gga time err count
     */
     static int ggacnt;

     /**
      * rmc time err count 
      */
     static int rmccnt; 
     
};

}}	// namespace nidas namespace dynld

#endif
