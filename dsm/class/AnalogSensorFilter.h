/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: $

    $LastChangedRevision:$

    $LastChangedBy: $

    $HeadURL: $
 ********************************************************************

*/

#ifndef DSM_ANALOGSAMPLEFILTER_H
#define DSM_ANALOGSAMPLEFILTER_H

#include <DSMAnalogSensor.h>
#include <Sample.h>
#include <SampleClient.h>
#include <SampleSource.h>

namespace dsm {

/**
 */
class AnalogSensorFilter: public SampleClient,
	public SampleSource {
public:

    AnalogSensorFilter();
    virtual ~AnalogSensorFilter();

    void setAnalogSensor(DSMAnalogSensor* val);

    /**
     * SampleClient receive method.
     */
    bool receive(const dsm::Sample* samp)
    	throw(dsm::SampleParseException,atdUtil::IOException);

protected:

private:

    int* rates;

};

}

#endif
