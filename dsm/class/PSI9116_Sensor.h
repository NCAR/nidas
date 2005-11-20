/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef PSI9116_SENSOR_H
#define PSI9116_SENSOR_H

#include <SocketSensor.h>

namespace dsm {

/**
 * Support for sampling a PSI 9116 pressure scanner from EsterLine
 * Pressure Systems.  This is a networked sensor, accepting connections
 * on TCP port 9000 and capable of receiving UDP broadcasts on port 7000.
 * Currently this class does not use UDP.
 */
class PSI9116_Sensor: public SocketSensor
{
public:

    PSI9116_Sensor();

    ~PSI9116_Sensor() { }

    void open(int flags)
    	throw(atdUtil::IOException,atdUtil::InvalidParameterException);

    void addSampleTag(SampleTag* stag)
            throw(atdUtil::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();


    void PSI9116_Sensor::purge(int msec) throw(atdUtil::IOException);


protected:

    std::string sendCommand(const std::string& cmd,int readlen = 0)
    	throw(atdUtil::IOException);

    int msecPeriod;

    /**
     * Number of sampled channels.
     */
    int nchannels;

    dsm_sample_id_t sampleId;

    /**
     * Conversion factor to apply to PSI data. 
     * PSI9116 by default reports data in psi.
     * A factor 68.94757 will convert to millibars.
     */
    float psiConvert;

    unsigned long sequenceNumber;

    size_t outOfSequence;

};

}

#endif
