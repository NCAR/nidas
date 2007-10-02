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
#include <nidas/core/RTL_IODevice.h>
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
    A2DSensor(),rtlinux(-1)
{
    setLatency(0.1);
}

DSC_A2DSensor::~DSC_A2DSensor()
{
}

bool DSC_A2DSensor::isRTLinux() const
{
    if (rtlinux < 0)  {
        const string& dname = getDeviceName();
        string::size_type fs = dname.rfind('/');
        if (fs != string::npos && (fs + 10) < dname.length() &&
            dname.substr(fs+1,10) == "rtldsc_a2d")
                    rtlinux = 1;
        else rtlinux = 0;
    }
    return rtlinux == 1;
}

IODevice* DSC_A2DSensor::buildIODevice() throw(n_u::IOException)
{
    if (isRTLinux()) return new RTL_IODevice();
    else return new UnixIODevice();
}

SampleScanner* DSC_A2DSensor::buildSampleScanner()
{
    return new SampleScanner();
}

void DSC_A2DSensor::open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException)
{
    DSMSensor::open(flags);
    init();

    int nchans = 0;

    ioctl(NIDAS_A2D_GET_NCHAN,&nchans,sizeof(nchans));

    if (_channels.size() > (unsigned)nchans) {
        ostringstream ost;
        ost << "max channel number is " << nchans;
        throw n_u::InvalidParameterException(getName(),"open",ost.str());
    }

    struct nidas_a2d_config cfg;
    unsigned int chan;
    for(chan = 0; chan < _channels.size(); chan++)
    {
	cfg.gain[chan] = _channels[chan].gain;
	cfg.bipolar[chan] = _channels[chan].bipolar;
	cfg.sampleIndex[chan] = _channels[chan].index;

#ifdef DEBUG
	cerr << "chan=" << chan << " rate=" << cfg.rate[chan] <<
		" gain=" << cfg.gain[chan] << 
		" bipolar=" << cfg.bipolar[chan] << endl;
#endif
    }
    for( ; chan < MAX_A2D_CHANNELS; chan++) {
	cfg.gain[chan] = 0;
	cfg.bipolar[chan] = false;
    }

    cfg.latencyUsecs = (int)(USECS_PER_SEC * getLatency());
    if (cfg.latencyUsecs == 0) cfg.latencyUsecs = USECS_PER_SEC / 10;

    cfg.scanRate = getScanRate();

    // cerr << "doing NIDAS_A2D_SET_CONFIG" << endl;
    ioctl(NIDAS_A2D_SET_CONFIG, &cfg, sizeof(struct nidas_a2d_config));

    for(unsigned int i = 0; i < _samples.size(); i++) {
        struct nidas_a2d_filter_config fcfg;

        fcfg.index = _samples[i].index;
	fcfg.rate = _samples[i].rate;
	fcfg.filterType = _samples[i].filterType;
	fcfg.boxcarNpts = _samples[i].boxcarNpts;

        ioctl(NIDAS_A2D_ADD_FILTER, &fcfg,
            sizeof(struct nidas_a2d_filter_config));
    }
    // cerr << "doing DMMAT_A2D_START" << endl;
    ioctl(DMMAT_A2D_START,0,0);
}


void DSC_A2DSensor::close() throw(n_u::IOException)
{
    cerr << "doing DMMAT_A2D_STOP" << endl;
    ioctl(DMMAT_A2D_STOP,0,0);
    A2DSensor::close();
}

void DSC_A2DSensor::init() throw(n_u::InvalidParameterException)
{
    if (initialized) return;
    initialized = true;
}

void DSC_A2DSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

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
}

void DSC_A2DSensor::addSampleTag(SampleTag* tag)
        throw(n_u::InvalidParameterException)
{

    int sindex = _samples.size();       // sample index, 0,1,...
    A2DSensor::addSampleTag(tag);

    struct sample_info* sinfo = &_samples[sindex];

    const vector<const Variable*>& vars = tag->getVariables();

    vector<const Variable*>::const_iterator vi;
    int ivar = 0;
    int prevChan = _channels.size() - 1;
    int lastChan = -1;

    for (vi = vars.begin(); vi != vars.end(); ++vi,ivar++) {

	const Variable* var = *vi;

	float fgain = 0.0;
	bool bipolar = true;
        /*
         * default ordering of channels is 0,1,2, etc.
         * The "channel" parameter can change this.
         * However the channels numbers within a sample
         * must be increasing.
         */
	int ichan = prevChan + 1;
	float corSlope = 1.0;
	float corIntercept = 0.0;

        const std::list<const Parameter*>& vparams = var->getParameters();
        list<const Parameter*>::const_iterator pi;
	for (pi = vparams.begin(); pi != vparams.end(); ++pi) {
	    const Parameter* param = *pi;
	    const string& pname = param->getName();
	    if (pname == "gain") {
		if (param->getLength() != 1)
		    throw n_u::InvalidParameterException(getName(),
		    	pname,"no value");
			
		fgain = param->getNumericValue(0);
	    }
	    else if (pname == "bipolar") {
		if (param->getLength() != 1)
		    throw n_u::InvalidParameterException(getName(),
		    	pname,"no value");
		bipolar = param->getNumericValue(0) != 0;
	    }
	    else if (pname == "channel") {
		if (param->getLength() != 1)
		    throw n_u::InvalidParameterException(getName(),pname,"no value");
		ichan = (int)param->getNumericValue(0);
	    }
	    else if (pname == "corSlope") {
		if (param->getLength() != 1)
		    throw n_u::InvalidParameterException(getName(),
		    	pname,"no value");
		corSlope = param->getNumericValue(0);
	    }
	    else if (pname == "corIntercept") {
		if (param->getLength() != 1)
		    throw n_u::InvalidParameterException(getName(),
		    	pname,"no value");
		corIntercept = param->getNumericValue(0);
	    }
	}
        assert(MAX_DMMAT_A2D_CHANNELS <= MAX_A2D_CHANNELS);
	if (ichan >= MAX_DMMAT_A2D_CHANNELS) {
	    ostringstream ost;
	    ost << MAX_DMMAT_A2D_CHANNELS;
	    throw n_u::InvalidParameterException(getName(),"variable",
		string("cannot sample more than ") + ost.str() +
		    string(" channels"));
	}
        if (ichan <= lastChan) {
	    ostringstream ost;
	    ost << MAX_DMMAT_A2D_CHANNELS;
	    throw n_u::InvalidParameterException(getName(),"variable",
		string("channel number ") + ost.str() + " is out of order in the sample");
        }
        prevChan = ichan;
        lastChan = ichan;

	if (fmodf(fgain,1.0) != 0.0) {
	    ostringstream ost;
	    ost << fgain;
	    throw n_u::InvalidParameterException(getName(),
		    "gain must be an integer",ost.str());
	}
	int gain = (int)fgain;

	struct chan_info ci;
	memset(&ci,0,sizeof(ci));
        ci.index = -1;
	for (int i = _channels.size(); i <= ichan; i++)
            _channels.push_back(ci);

	ci = _channels[ichan];

	if (ci.gain > 0) {
	    ostringstream ost;
	    ost << ichan;
	    throw n_u::InvalidParameterException(getName(),"variable",
		string("multiple variables for A2D channel ") + ost.str());
	}

	ci.gain = gain;
	ci.bipolar = bipolar;
        ci.index = sindex;
	_channels[ichan] = ci;

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
	 *
	 * corSlope and corIntercept are the slope and intercept
	 * of an A2D calibration, where
	 *    Vcorr = Vuncorr * corSlope + corIntercept
	 *
	 * Note that Vcorr is the Y (independent) variable. This is
	 * because the A2D calibration is done in a similar
	 * way to normal sensor calibration, where Y are the
	 * set points from an input standard, and X is the measured
	 * voltage value.
	 *
	 *    Vcorr = Vuncorr * corSlope + corIntercept
	 *	    = (cnts * 20 / 65535 / gain + offset) * corSlope +
	 *			corIntercept
	 *	    = cnts * 20 / 65535 / gain * corSlope +
	 *		offset * corSlope + corIntercept
	 */

	sinfo->convSlopes[ivar] = 20.0 / 65535 / fgain * corSlope;
	if (bipolar) 
	    sinfo->convIntercepts[ivar] = corIntercept;
        else 
	    sinfo->convIntercepts[ivar] = corIntercept +
	    	10.0 / fgain * corSlope;
    }
}

void DSC_A2DSensor::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    DSMSensor::fromDOMElement(node);
}

