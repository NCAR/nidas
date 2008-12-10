/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-03-16 23:40:37 -0600 (Fri, 16 Mar 2007) $

    $LastChangedRevision: 3736 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/DSC_FreqCounter.cc $

 ******************************************************************
*/

#include <nidas/dynld/DSC_FreqCounter.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/linux/diamond/gpio_mm.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(DSC_FreqCounter)

DSC_FreqCounter::DSC_FreqCounter() :
    DSMSensor(),_sampleId(0),_nvars(0),
    _msecPeriod(MSECS_PER_SEC),_numPulses(0),
    _clockRate(GPIO_MM_CT_CLOCK_HZ),
    _cvtr(0)
{
    setLatency(0.1);
}

DSC_FreqCounter::~DSC_FreqCounter()
{
}

bool DSC_FreqCounter::isRTLinux() const
{
    return false;
}

IODevice* DSC_FreqCounter::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner* DSC_FreqCounter::buildSampleScanner()
{
    return new DriverSampleScanner();
}

void DSC_FreqCounter::open(int flags) throw(n_u::IOException,
    n_u::InvalidParameterException)
{
    DSMSensor::open(flags);

    init();

    struct GPIO_MM_fcntr_config cfg;
    cfg.outputPeriodUsec = _msecPeriod * USECS_PER_MSEC;
    cfg.latencyUsecs = 250 * USECS_PER_MSEC;
    cfg.numPulses = _numPulses;
    ioctl(GPIO_MM_FCNTR_START,&cfg,sizeof(cfg));
}

void DSC_FreqCounter::init() throw(n_u::InvalidParameterException)
{
    DSMSensor::init();
    readParams(getParameters());

    /* The driver is designed such that each device,
     * /dev/gpiomm_freqN provides one frequency measurement.
     */
    if (getSampleTags().size() != 1)
        throw n_u::InvalidParameterException(getName(),"sample",
            "must have exactly one sample");
    const SampleTag* stag = *getSampleTags().begin();

    _sampleId = stag->getId();
    _nvars = stag->getVariables().size();
    switch (_nvars) {
    case 3:
    case 2:
    case 1:
        break;
    default:
        throw n_u::InvalidParameterException(getName(),"variable",
            "sample must contain one or two variables");
    }

    _msecPeriod =  (int)rint(MSECS_PER_SEC / stag->getRate());

    readParams(stag->getParameters());

    _cvtr = n_u::EndianConverter::getConverter(
        n_u::EndianConverter::EC_LITTLE_ENDIAN);
    
}

void DSC_FreqCounter::readParams(const list<const Parameter*>& params)
    throw(n_u::InvalidParameterException)
{
    list<const Parameter*>::const_iterator pi = params.begin();
    for (pi = params.begin(); pi != params.end(); ++pi) {
        const Parameter* p = *pi;
        if (p->getName() == "NumPulses") {
            if (p->getType() != Parameter::INT_PARAM || p->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),
                    "NumPulses","should be a integer of length 1");
             _numPulses = (int)rint(p->getNumericValue(0));
        }
        else if (p->getName() == "ClockRate") {
            if (p->getType() != Parameter::INT_PARAM || p->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),
                    "ClockRate","should be a integer of length 1");
             _clockRate = p->getNumericValue(0);
             // cerr << "_clockRate=" << _clockRate << endl;
        }
    }
}

void DSC_FreqCounter::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
	return;
    }

    struct GPIO_MM_fcntr_status stat;
    try {
	ioctl(GPIO_MM_FCNTR_GET_STATUS,&stat,sizeof(stat));
	ostr << "<td align=left>";
	ostr << "lostSamples=" << stat.lostSamples <<
		", pulse underflow=" << stat.pulseUnderflow <<
		", bad gate=" << stat.badGateWarning;
	ostr << "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: printStatus: %s",getName().c_str(),
	    ioe.what());
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}

double DSC_FreqCounter::calculatePeriodUsec(const Sample* insamp) const
{
    // data is two 4 byte integers.
    if (insamp->getDataByteLength() != 2 * sizeof(int)) return floatNAN;
    const unsigned int* ip =
            (const unsigned int*)insamp->getConstVoidDataPtr();
    unsigned int pulses = _cvtr->uint32Value(ip);
    unsigned int tics = _cvtr->uint32Value(ip+1);

    return calculatePeriodUsec(pulses,tics);
}

double DSC_FreqCounter::calculatePeriodUsec(unsigned int pulses, unsigned int tics) const
{
    if (pulses == 0) return floatNAN;	// actually infinity
    if (tics == 0) return 0.0;
    return (double) tics / pulses / _clockRate * USECS_PER_SEC;
}

bool DSC_FreqCounter::process(const Sample* insamp,list<const Sample*>& results)
    throw()
{
    // data is two 4 byte integers.
    if (insamp->getDataByteLength() != 2 * sizeof(int32_t)) return false;

    SampleT<float>* osamp = getSample<float>(_nvars);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(_sampleId);
    float *fp = osamp->getDataPtr();

    const unsigned int* ip =
            (const unsigned int*)insamp->getConstVoidDataPtr();
    unsigned int pulses = _cvtr->uint32Value(ip);
    unsigned int tics = _cvtr->uint32Value(ip+1);

    double usec = calculatePeriodUsec(pulses,tics);

    switch (_nvars) {
    case 3:
        fp[2] = floatNAN;
    case 2:
        fp[1] = USECS_PER_SEC / usec;
    case 1:
        fp[0] = (float) usec;
        break;
    }

    results.push_back(osamp);

    return true;
}

