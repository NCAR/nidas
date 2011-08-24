/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/dynld/DSC_A2DSensor.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/linux/diamond/dmd_mmat.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(DSC_A2DSensor)

DSC_A2DSensor::DSC_A2DSensor() :
    A2DSensor()
{
    setLatency(0.1);
}

DSC_A2DSensor::~DSC_A2DSensor()
{
}

IODevice* DSC_A2DSensor::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner* DSC_A2DSensor::buildSampleScanner()
    	throw(nidas::util::InvalidParameterException)
{
    return new DriverSampleScanner();
}

void DSC_A2DSensor::open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException)
{
    A2DSensor::open(flags);

    // Get the actual number of input channels on the card.
    // This depends on differential/single-ended jumpering
    int nchan;

    ioctl(NIDAS_A2D_GET_NCHAN,&nchan,sizeof(nchan));

    struct nidas_a2d_config cfg;
    cfg.scanRate = getScanRate();
    cfg.latencyUsecs = (int)(USECS_PER_SEC * getLatency());
    if (cfg.latencyUsecs == 0) cfg.latencyUsecs = USECS_PER_SEC / 10;

    ioctl(NIDAS_A2D_SET_CONFIG, &cfg, sizeof(cfg));

    for(unsigned int i = 0; i < _sampleCfgs.size(); i++) {
        struct nidas_a2d_sample_config* scfg = _sampleCfgs[i];
    
        for (int j = 0; j < scfg->nvars; j++) {
            if (scfg->channels[j] >= nchan) {
                ostringstream ost;
                ost << "channel number " << scfg->channels[j] <<
                    " is out of range, max=" << nchan;
                throw n_u::InvalidParameterException(getName(),
                    "channel",ost.str());
            }
        }
        ioctl(NIDAS_A2D_CONFIG_SAMPLE, scfg,
            sizeof(struct nidas_a2d_sample_config)+scfg->nFilterData);
    }

    ioctl(DMMAT_START,0,0);
}


void DSC_A2DSensor::close() throw(n_u::IOException)
{
    ioctl(DMMAT_STOP,0,0);
    A2DSensor::close();
}

void DSC_A2DSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
	return;
    }

    struct DMMAT_A2D_Status stat;
    try {
	ioctl(DMMAT_A2D_GET_STATUS,&stat,sizeof(stat));
	ostr << "<td align=left>";
	ostr << "FIFO: over=" << stat.fifoOverflows << 
		", under=" << stat.fifoUnderflows <<
		", lostSamples=" << stat.missedSamples;
	ostr << "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: printStatus: %s",getName().c_str(),
	    ioe.what());
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}

void DSC_A2DSensor::addSampleTag(SampleTag* tag)
        throw(n_u::InvalidParameterException)
{
    A2DSensor::addSampleTag(tag);
}

void DSC_A2DSensor::setA2DParameters(int ichan,int gain,int bipolar)
            throw(n_u::InvalidParameterException)
{
    if (ichan < 0 || ichan >= MAX_DMMAT_A2D_CHANNELS) {
        ostringstream ost;
        ost << "value=" << ichan << " doesn't exist";
        throw n_u::InvalidParameterException(getName(), "channel",ost.str());
    }
        
    // screen invalid gains
    switch(gain) {
    case 1:
        if (!bipolar) throw n_u::InvalidParameterException(getName(),
            "gain,bipolar","gain of 1 and bipolar=F is not supported");
        break;
    case 2:
    case 4:
    case 8:
    case 16:
        break;
    default:
        {
            ostringstream ost;
            ost << "value=" << gain << " is not supported";
            throw n_u::InvalidParameterException(getName(),"gain",ost.str());
        }
    }
    A2DSensor::setA2DParameters(ichan,gain,bipolar);

}

void DSC_A2DSensor::getBasicConversion(int ichan,
    float& intercept, float& slope) const
{
    /*
     * DSC returns (32767) for maximum voltage of range and -32768 for
     * minimum voltage.
     *
     * For bipolar=T 
     *	cnts = ((V * gain / 20) * 65535 =
     *	So:   V = cnts / 65535 * 20 / gain
     * For bipolar=F 
     *	cnts = ((V * gain / 20) * 65535 - 32768
     *	So:   V = (cnts + 32768) / 65535 * 20 / gain
     *              = cnts / 65535 * 20 / gain + 10 / gain
     *
     * V = cnts / 65535 * 20 / gain + offset
     *	where offset = for 0 bipolar, and 10/gain for unipolar.
     */
    slope = 20.0 / 65535.0 / getGain(ichan);
    if (getBipolar(ichan)) intercept = 0.0;
    else intercept = 10.0 / getGain(ichan);
}


void DSC_A2DSensor::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    A2DSensor::fromDOMElement(node);
}

