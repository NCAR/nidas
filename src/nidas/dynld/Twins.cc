/*
 ******************************************************************
    Copyright 2010 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: January 2, 2011 $

    $LastChangedRevision: $

    $LastChangedBy: ryano $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/Twins.cc $

 ******************************************************************
*/

#include <nidas/dynld/Twins.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/linux/diamond/dmd_mmat.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(Twins)

Twins::Twins() :
    A2DSensor()
{
    setLatency(0.1);
}

Twins::~Twins()
{
}

//bool Twins::isRTLinux() const
//{
    //const string& dname = getDeviceName();
    //string::size_type fs = dname.rfind('/');
    //if (fs != string::npos && (fs + 10) < dname.length() &&
        //dname.substr(fs+1,10) == "rtldsc_a2d")
                //rtlinux = 1;
    ////else rtlinux = 0;

    ////return rtlinux == 1;
//}

IODevice* Twins::buildIODevice() throw(n_u::IOException)
{
    //if (isRTLinux()) return new RTL_IODevice();
    //else return new UnixIODevice();
    return new UnixIODevice();
}

SampleScanner* Twins::buildSampleScanner()
    	throw(nidas::util::InvalidParameterException)
{
    return new DriverSampleScanner();
}

void Twins::open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException)
{
    A2DSensor::open(flags);

    int size = 256;
    struct waveform *wave, *wave2, *wave3;
    int waveform[size], waveform2[size], waveform3[size];

    // Send along the number of channels and the desired waveform rate
    // in Hertz (how many complete waveforms to send out per second).
    // All waveforms are output at the same rate.
    struct D2D_Config cfg = {3, 50};
    ioctl(DMMAT_D2D_CONFIG, &cfg, sizeof(cfg));

    for(int i = 0; i < size; i++){
        waveform[i] = i*7;
        waveform2[i] = 4000 - 7*i;
        waveform3[i] = i*3;
    }

    // This is a template for how to build a waveform.
    wave = (struct waveform*) malloc(sizeof(struct waveform) + sizeof(int)*size );
    memcpy(&wave->point, waveform, sizeof(int)*size);
    wave->channel = 0;
    wave->size = size;

    // How to send a waveform
    //printf("Sending wave (i*7)\n");
    n_u::Logger::getInstance()->log(LOG_WARNING,
            "%s: Sending wave (i*7)",getName().c_str());
    ioctl(DMMAT_ADD_WAVEFORM, wave, sizeof(*wave));

    // Second wave example
    wave2 = (struct waveform*) malloc(sizeof(struct waveform) + sizeof(int)*size );
    memcpy(&wave2->point, waveform2, sizeof(int)*size);
    wave2->channel = 1;
    wave2->size = size;

    //printf("Sending wave 2 (4000-i*7)\n");
    n_u::Logger::getInstance()->log(LOG_WARNING,
            "%s: Sending wave (4000-i*7)",getName().c_str());
    ioctl(DMMAT_ADD_WAVEFORM, wave2, sizeof(*wave2));

    // Third wave example
    wave3 = (struct waveform*) malloc(sizeof(struct waveform) + sizeof(int)*size );
    memcpy(&wave3->point, waveform3, sizeof(int)*size);
    wave3->channel = 2;
    wave3->size = size;

    //printf("Sending wave 3 (i*3)\n");
    n_u::Logger::getInstance()->log(LOG_WARNING,
            "%s: Sending wave (i*3)",getName().c_str());
    ioctl(DMMAT_ADD_WAVEFORM, wave3, sizeof(*wave3));

    // Tell the D2D device to start.
    ioctl(DMMAT_D2D_START,0,0);

    free(wave);
    free(wave2);
    free(wave3);



    /*
    * This stuff appears to be a copy of DSC_A2DSensor which
    * is perhaps applicable a little later
    *

    // Get the actual number of input channels on the card.
    // This depends on differential/single-ended jumpering
    //
    // DONT NEED?
    int nchan;

    //ioctl(NIDAS_A2D_GET_NCHAN,&nchan,sizeof(nchan));

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

    ioctl(DMMAT_A2D_START,0,0);
    */
}


void Twins::close() throw(n_u::IOException)
{
    //ioctl(DMMAT_A2D_STOP,0,0);
    ioctl(DMMAT_D2D_STOP,0,0);
    A2DSensor::close();
}

void Twins::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
	return;
    }

   ostr << "Twins::printStatus() called" << endl;
   return;
/*
 * Appears to be a direct copy of DSC_A2DSensor:: printStatus
 * could be handy later

    struct DMMAT_A2D_Status stat;
    try {
	ioctl(DMMAT_A2D_GET_STATUS,&stat,sizeof(stat));
	ostr << "<td align=left>";
	ostr << "FIFO: over=" << stat.fifoOverflows << 
		", under=" << stat.fifoUnderflows <<
		", not empty=" << stat.fifoNotEmpty <<
		", lostSamples=" << stat.missedSamples <<
		", irqs=" << stat.irqsReceived;
	ostr << "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: printStatus: %s",getName().c_str(),
	    ioe.what());
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
*/
}

void Twins::addSampleTag(SampleTag* tag)
        throw(n_u::InvalidParameterException)
{
    A2DSensor::addSampleTag(tag);
}

void Twins::setA2DParameters(int ichan,int gain,int bipolar)
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

void Twins::getBasicConversion(int ichan,
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


void Twins::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    A2DSensor::fromDOMElement(node);
}

