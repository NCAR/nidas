/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef GPS_NMEA_SERIAL_H
#define GPS_NMEA_SERIAL_H

#include <DSMSerialSensor.h>

/**
 * Id of sample from GGA NMEA record.  Fixed at 1.
 */
#define GGA_SAMPLE_ID 1

/**
 * Id of sample from RMC NMEA record.  Fixed at 2.
 */
#define RMC_SAMPLE_ID 2

namespace dsm {

/**
 * A class for reading NMEA records from a GPS attached to
 * a serial port.
 */
class GPS_NMEA_Serial: public DSMSerialSensor
{
public:

    GPS_NMEA_Serial():DSMSerialSensor(),inputStr(0),inputStrLen(0) {}

    ~GPS_NMEA_Serial() { delete [] inputStr; }

    void addSampleTag(SampleTag* stag)
            throw(atdUtil::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

protected:
    void parseGGA(const char* input,const char* eoi,float *dout,int nvars) 
    	throw();
  
    void parseRMC(const char* input,const char* eoi,float *dout,int nvars)
    	throw();

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
     * copy of input string, null terminated.
     */
    char* inputStr;

    int inputStrLen;

};

}

#endif
