// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/dynld/raf/A2DBoardTempSensor.h>
#include <nidas/linux/ncar_a2d.h>

#include <nidas/core/DSMEngine.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, A2DBoardTempSensor)

A2DBoardTempSensor::A2DBoardTempSensor() :
    DSMSensor(), sampleId(0), rate(IRIG_1_HZ),
    DEGC_PER_CNT(0.0625)
{
}

A2DBoardTempSensor::~A2DBoardTempSensor()
{
}

IODevice* A2DBoardTempSensor::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner* A2DBoardTempSensor::buildSampleScanner()
    throw(n_u::InvalidParameterException)
{
    setDriverTimeTagUsecs(USECS_PER_MSEC);
    return new DriverSampleScanner();
}

void A2DBoardTempSensor::open(int /* flags */)
	throw(n_u::IOException, n_u::InvalidParameterException)
{
    throw n_u::IOException(getName(),"open","obsolete");
}

void A2DBoardTempSensor::close() throw(n_u::IOException)
{
    throw n_u::IOException(getName(),"close","obsolete");
}

void A2DBoardTempSensor::init() throw(n_u::InvalidParameterException)
{
    DSMSensor::init();
    for (SampleTagIterator ti = getSampleTagIterator(); ti.hasNext(); ) {
	const SampleTag* tag = ti.next();
	rate = irigClockRateToEnum((int)tag->getRate());
	sampleId = tag->getId();
	break;
    }
}

bool A2DBoardTempSensor::process(const Sample* insamp, list<const Sample*>& result) throw()
{
    // number of data values in this raw sample. Should be one.
    if (insamp->getDataByteLength() / sizeof(short) != 1) return false;

    // pointer to 16 bit raw temperature
    const signed short* sp = (const signed short*)
    	insamp->getConstVoidDataPtr();

    SampleT<float>* osamp = getSample<float>(1);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(sampleId);
    osamp->getDataPtr()[0] = *sp * DEGC_PER_CNT;

    result.push_back(osamp);
    return true;
}


