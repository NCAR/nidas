/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision:$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_ANALOGSAMPLEFILTER_H
#define DSM_ANALOGSAMPLEFILTER_H

#include <DSMAnalogSensor.h>
#include <Sample.h>
#include <SampleClient.h>
#include <SampleSource.h>
#include <a2d_driver.h>

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
	void initPtrs();

	int* rates;
	int  xfrctr;	// Keep track of number of transfers
	long *sumbuf;
	int a2dptr[MAXA2DS];
	int a2dctr[MAXA2DS];
	float norm[MAXA2DS];
	int Ictr;
	long outsize;
};

}

#endif
