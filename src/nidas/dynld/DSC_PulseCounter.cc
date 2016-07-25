// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "DSC_PulseCounter.h"
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
    DSMSensor(),_sampleId(0),_msecPeriod(MSECS_PER_SEC),
    _cvtr(0)
{
    setLatency(0.1);
}

DSC_PulseCounter::~DSC_PulseCounter()
{
}

IODevice* DSC_PulseCounter::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner* DSC_PulseCounter::buildSampleScanner()
    throw(n_u::InvalidParameterException)
{
    return new DriverSampleScanner();
}

void DSC_PulseCounter::open(int flags) throw(n_u::IOException,
    n_u::InvalidParameterException)
{
    DSMSensor::open(flags);

    init();

    struct DMMAT_CNTR_Config cfg;
    cfg.msecPeriod = _msecPeriod;
    ioctl(DMMAT_CNTR_START,&cfg,sizeof(cfg));
}


void DSC_PulseCounter::close() throw(n_u::IOException)
{
    ioctl(DMMAT_CNTR_STOP,0,0);
    DSMSensor::close();
}

void DSC_PulseCounter::validate() throw(n_u::InvalidParameterException)
{
    DSMSensor::validate();

    list<SampleTag*>& tags = getSampleTags();
    if (tags.size() != 1)
        throw n_u::InvalidParameterException(getName(),"sample",
            "must have exactly one sample");
    const SampleTag* stag = *tags.begin();

    if (stag->getVariables().size() != 1)
        throw n_u::InvalidParameterException(getName(),"variable",
            "sample must contain exactly one variable");
    _sampleId = stag->getId();

    _msecPeriod =  (int)rint(MSECS_PER_SEC / stag->getRate());

}
void DSC_PulseCounter::init() throw(n_u::InvalidParameterException)
{
    DSMSensor::init();
    _cvtr = n_u::EndianConverter::getConverter(
        n_u::EndianConverter::EC_LITTLE_ENDIAN);

}

void DSC_PulseCounter::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
	return;
    }

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

