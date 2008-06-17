/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-03-16 23:40:37 -0600 (Fri, 16 Mar 2007) $

    $LastChangedRevision: 3736 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/DSC_PulseCounter.cc $

 ******************************************************************
*/

#include <nidas/dynld/DSC_PulseCounter.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/linux/diamond/dmd_mmat.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(DSC_PulseCounter)

DSC_PulseCounter::DSC_PulseCounter() :
    DSMSensor(),sampleId(0),msecPeriod(MSECS_PER_SEC),
    cvtr(0)
{
    setLatency(0.1);
}

DSC_PulseCounter::~DSC_PulseCounter()
{
}

bool DSC_PulseCounter::isRTLinux() const
{
    return false;
}

IODevice* DSC_PulseCounter::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner* DSC_PulseCounter::buildSampleScanner()
{
    return new DriverSampleScanner();
}

void DSC_PulseCounter::open(int flags) throw(n_u::IOException,
    n_u::InvalidParameterException)
{
    DSMSensor::open(flags);

    init();

    struct DMMAT_CNTR_Config cfg;
    cfg.msecPeriod = msecPeriod;
    ioctl(DMMAT_CNTR_START,&cfg,sizeof(cfg));
}


void DSC_PulseCounter::close() throw(n_u::IOException)
{
    ioctl(DMMAT_CNTR_STOP,0,0);
    DSMSensor::close();
}


void DSC_PulseCounter::init() throw(n_u::InvalidParameterException)
{
    if (getSampleTags().size() != 1)
        throw n_u::InvalidParameterException(getName(),"sample",
            "must have exactly one sample");
    const SampleTag* stag = *getSampleTags().begin();

    if (stag->getVariables().size() != 1)
        throw n_u::InvalidParameterException(getName(),"variable",
            "sample must contain exactly one variable");
    sampleId = stag->getId();

    msecPeriod =  (int)rint(MSECS_PER_SEC / stag->getRate());

    cvtr = n_u::EndianConverter::getConverter(
        n_u::EndianConverter::EC_LITTLE_ENDIAN);
}

void DSC_PulseCounter::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    struct DMMAT_CNTR_Status stat;
    try {
	ioctl(DMMAT_CNTR_GET_STATUS,&stat,sizeof(stat));
	ostr << "<td align=left>";
	ostr << "lostSamples=" << stat.lostSamples <<
		", irqs=" << stat.irqsReceived;
	ostr << "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: printStatus: %s",getName().c_str(),
	    ioe.what());
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}

bool DSC_PulseCounter::process(const Sample* insamp,list<const Sample*>& results)
    throw()
{
    // count is a four byte, little endian integer.
    if (insamp->getDataByteLength() != sizeof(long)) return false;

    SampleT<float>* osamp = getSample<float>(1);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(sampleId);
    float *fp = osamp->getDataPtr();

    // Note: we lose digits here when converting
    // from unsigned long to floats.
    *fp = (float)cvtr->uint32Value(insamp->getConstVoidDataPtr());

    results.push_back(osamp);

    return true;
}

