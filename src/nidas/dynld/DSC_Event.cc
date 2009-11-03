/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/dynld/DSC_Event.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/linux/diamond/gpio_mm.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(DSC_Event)

DSC_Event::DSC_Event() :
    DSMSensor(),_sampleId(0),_cvtr(0)
{
    setLatency(0.25);
}

DSC_Event::~DSC_Event()
{
}

bool DSC_Event::isRTLinux() const
{
    return false;
}

IODevice* DSC_Event::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner* DSC_Event::buildSampleScanner()
    throw(n_u::InvalidParameterException)
{
    return new DriverSampleScanner();
}

void DSC_Event::open(int flags) throw(n_u::IOException,
    n_u::InvalidParameterException)
{
    DSMSensor::open(flags);

    init();

    struct GPIO_MM_event_config cfg;
    cfg.latencyUsecs = (int) rint(getLatency() * USECS_PER_SEC);
    ioctl(GPIO_MM_EVENT_START,&cfg,sizeof(cfg));
}


void DSC_Event::close() throw(n_u::IOException)
{
    DSMSensor::close();
}


void DSC_Event::init() throw(n_u::InvalidParameterException)
{
    DSMSensor::init();
    if (getSampleTags().size() != 1)
        throw n_u::InvalidParameterException(getName(),"sample",
            "must have exactly one sample");
    const SampleTag* stag = *getSampleTags().begin();

    if (stag->getVariables().size() != 1)
        throw n_u::InvalidParameterException(getName(),"variable",
            "sample must contain exactly one variable");
    _sampleId = stag->getId();

    _cvtr = n_u::EndianConverter::getConverter(
        n_u::EndianConverter::EC_LITTLE_ENDIAN);
}

void DSC_Event::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
	return;
    }

    struct GPIO_MM_event_status stat;
    try {
	ioctl(GPIO_MM_EVENT_GET_STATUS,&stat,sizeof(stat));
	ostr << "<td align=left>";
	ostr << "lostSamples=" << stat.lostSamples <<
		", nevents=" << stat.nevents;
	ostr << "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: printStatus: %s",getName().c_str(),
	    ioe.what());
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}

bool DSC_Event::process(const Sample* insamp,list<const Sample*>& results)
    throw()
{
    // count is a four byte, little endian integer.
    if (insamp->getDataByteLength() != sizeof(int)) return false;

    SampleT<float>* osamp = getSample<float>(1);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(_sampleId);
    float *fp = osamp->getDataPtr();

    // Note: we lose digits here when converting
    // from unsigned int to floats.
    *fp = (float)_cvtr->uint32Value(insamp->getConstVoidDataPtr());

    results.push_back(osamp);

    return true;
}

