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
    DSMSensor(),initialized(false),
    scanRate(0),badRawSamples(0),
    rtlinux(-1)
{
    setLatency(0.1);
}

DSC_A2DSensor::~DSC_A2DSensor()
{
    for (unsigned int i = 0; i < samples.size(); i++) {
        delete [] samples[i].convSlopes;
        delete [] samples[i].convIntercepts;
    }
}

bool DSC_A2DSensor::isRTLinux() const
{
    if (rtlinux < 0)  {
        const string& dname = getDeviceName();
        unsigned int fs = dname.rfind('/');
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

void DSC_A2DSensor::open(int flags) throw(n_u::IOException)
{
    DSMSensor::open(flags);

    init();

    int nchans;
    ioctl(DMMAT_GET_A2D_NCHAN,&nchans,sizeof(int));

    if (channels.size() > (unsigned)nchans) {
        ostringstream ost;
        ost << "max channel number is " << nchans;
        throw n_u::IOException(getName(),"open",ost.str());
    }

    struct DMMAT_A2D_Config cfg;
    unsigned int chan;
    for(chan = 0; chan < channels.size(); chan++)
    {
	cfg.gain[chan] = channels[chan].gain;
	cfg.bipolar[chan] = channels[chan].bipolar;
	cfg.id[chan] = channels[chan].id;

#ifdef DEBUG
	cerr << "chan=" << chan << " rate=" << cfg.rate[chan] <<
		" gain=" << cfg.gain[chan] << 
		" bipolar=" << cfg.bipolar[chan] << endl;
#endif
    }
    for( ; chan < MAX_DMMAT_A2D_CHANNELS; chan++) {
	cfg.gain[chan] = 0;
	cfg.bipolar[chan] = false;
    }

    cfg.latencyUsecs = (int)(USECS_PER_SEC * getLatency());
    if (cfg.latencyUsecs == 0) cfg.latencyUsecs = USECS_PER_SEC / 10;

    cfg.scanRate = getScanRate();

    // cerr << "doing DMMAT_SET_A2D_CONFIG" << endl;
    ioctl(DMMAT_SET_A2D_CONFIG, &cfg, sizeof(struct DMMAT_A2D_Config));

    for(unsigned int i = 0; i < samples.size(); i++) {
        struct DMMAT_A2D_Sample_Config cfg;

        cfg.id = samples[i].id;
	cfg.rate = samples[i].rate;
	cfg.filterType = samples[i].filterType;
	cfg.boxcarNpts = samples[i].boxcarNpts;

        ioctl(DMMAT_SET_A2D_SAMPLE, &cfg,
            sizeof(struct DMMAT_A2D_Sample_Config));
    }
    // cerr << "doing DMMAT_A2D_START" << endl;
    ioctl(DMMAT_A2D_START,0,0);
}


void DSC_A2DSensor::close() throw(n_u::IOException)
{
    cerr << "doing DMMAT_A2D_STOP" << endl;
    ioctl(DMMAT_A2D_STOP,0,0);
    DSMSensor::close();
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
	ioctl(DMMAT_GET_A2D_STATUS,&stat,sizeof(stat));
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

bool DSC_A2DSensor::process(const Sample* insamp,list<const Sample*>& results) throw()
{
    // pointer to raw A2D counts
    const short* sp = (const short*) insamp->getConstVoidDataPtr();

    // raw data are shorts
    if (insamp->getDataByteLength() % sizeof(short)) {
        badRawSamples++;
        return false;
    }

    // number of short values in this raw sample.
    unsigned int nvalues = insamp->getDataByteLength() / sizeof(short);
    if (nvalues < 1) {
        badRawSamples++;
        return false;      // nothin
    }

    unsigned int id0 = 0;
    // if more than one sample, the first value is an index
    if (samples.size() != 1 || nvalues == samples[0].nvars + 1) {
        int id = *sp++;
        if (id < 0 || id >= (signed) samples.size()) {
            badRawSamples++;
            return false;
        }
        id0 = id;
        nvalues--;
    }

    struct sample_info* sinfo = &samples[id0];

    SampleT<float>* osamp = getSample<float>(sinfo->nvars);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(sinfo->sampleId);
    float *fp = osamp->getDataPtr();

    unsigned int ival;
    for (ival = 0; ival < std::min(nvalues,sinfo->nvars); ival++) {
	short sval = *sp++;
	float volts;
	if (sval == -32768 || sval == 32767) volts = floatNAN;
	else volts = sinfo->convIntercepts[ival] +
            sinfo->convSlopes[ival] * sval;
        *fp++ = volts;
    }

    for ( ; ival < sinfo->nvars; ival++) *fp++ = floatNAN;
    results.push_back(osamp);

    return true;
}

void DSC_A2DSensor::addSampleTag(SampleTag* tag)
        throw(n_u::InvalidParameterException)
{
    /*
     Hypothetical, somewhat complex sensor configuration:
     User wants to sample channels 0,4 and 5 at 10/s,
     and channels 1 and 2 at 100/s:

        <sensor>
            <parameter name="rate" value="1000"/>   // sample rate 1KHz
            <sample id="1" rate="10">               // output rate 10Hz
                <parameter name="filter" value="boxcar"/>
                <parameter name="numpoints" value="4"/>
                <variable name="IN0"/>  // default channel 0
                <variable name="IN4">
                   <parameter name="channel" value="4"/>
                </variable>
                <variable name="IN5"/>  // channel=5 is 1 plus previous
                </variable>
            </sample>
            <sample id="2" rate="100">              // output rate 100Hz
                <parameter name="filter" value="boxcar"/>
                <parameter name="numpoints" value="4"/>
                <variable name="IN1">
                   <parameter name="channel" value="1"/>
                </variable>
                <variable name="IN2"/>      // channel=2 is 1 plus previous
            </sample>
        </sensor>

     For the driver, we save this information in
     vector<struct chan_info> channels by channel number:
         channels[0]
            gain=X,bipolar=X,id=0 (id is one less than id="1")
         channels[1]
            gain=X,bipolar=X,id=1 (id is one less than id="2")
         channels[2]
            gain=X,bipolar=X,id=1 (id is one less than id="2")
         channels[3]
            gain=0,bipolar=0,id=0 (gain=0 means not sampled)
         channels[4]
            gain=X,bipolar=x,id=0 
         channels[5]
            gain=X,bipolar=X,id=0

     Also build vector<struct sample_info> samples:
     samples[0]
        id=0,rate=10,filterType=X,nvars=3,convSlopes=(m,m,m) convIntercepts=(b,b,b)
     samples[1]
        id=1,rate=100,filterType=X,nvars=2,convSlopes=(m,m) convIntercepts=(b,b)

     The process method will receive the individual samples. The first
     short int in the data is the id.
     For id==0, there should be 3 more short int values containing
     the A2D counts for variables IN0,IN4 and IN5. The data length
     of the raw sample should be 8 bytes.
     For id==1, there should be 2 more short int values containing
     the A2D counts for variables IN1,IN2, and the data length
     should be 6 bytes.

     If only one sample is configured, the driver does not add
     an id to the samples (backward compatibility)
     */
    DSMSensor::addSampleTag(tag);

    float frate = tag->getRate();
    if (fmodf(frate,1.0) != 0.0) {
	ostringstream ost;
	ost << frate;
        throw n_u::InvalidParameterException(getName(),
		"rate must be an integer",ost.str());
    }

    int rate = (int)frate;
    if (getScanRate() < rate) setScanRate(rate);
    int boxcarNpts = 1;

    enum nidas_short_filter filterType = NIDAS_FILTER_PICKOFF;
    const std::list<const Parameter*>& params = tag->getParameters();
    list<const Parameter*>::const_iterator pi;
    for (pi = params.begin(); pi != params.end(); ++pi) {
        const Parameter* param = *pi;
        const string& pname = param->getName();
        if (pname == "filter") {
                if (param->getType() != Parameter::STRING_PARAM ||
                    param->getLength() != 1)
                    throw n_u::InvalidParameterException(getName(),"sample",
                        "filter parameter is not a string");
                string fname = param->getStringValue(0);
                if (fname == "boxcar") filterType = NIDAS_FILTER_BOXCAR;
                else if (fname == "pickoff") filterType = NIDAS_FILTER_PICKOFF;
                else throw n_u::InvalidParameterException(getName(),"sample",
                        fname + " filter is not supported");
        }
        else if (pname == "numpoints") {
                if (param->getLength() != 1)
                    throw n_u::InvalidParameterException(getName(),"sample",
                        "bad sampleRate parameter");
                boxcarNpts = (int)param->getNumericValue(0);
        }
    }

    dsm_sample_id_t tagid = tag->getId();
    
    int id0 = samples.size();       // which sample

    const vector<const Variable*>& vars = tag->getVariables();

    sample_info sinfo;
    memset(&sinfo,0,sizeof(sinfo));
    sinfo.sampleId = tagid;
    sinfo.id = id0;
    sinfo.rate = rate;
    sinfo.filterType = filterType;
    sinfo.boxcarNpts = boxcarNpts;
    sinfo.nvars = vars.size();
    sinfo.convSlopes = new float[sinfo.nvars];
    sinfo.convIntercepts = new float[sinfo.nvars];
    samples.push_back(sinfo);

    vector<const Variable*>::const_iterator vi;
    int ivar = 0;
    int prevChan = channels.size() - 1;
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
	for (int i = channels.size(); i <= ichan; i++) channels.push_back(ci);

	ci = channels[ichan];

	if (ci.gain > 0) {
	    ostringstream ost;
	    ost << ichan;
	    throw n_u::InvalidParameterException(getName(),"variable",
		string("multiple variables for A2D channel ") + ost.str());
	}

	ci.gain = gain; 	// gain of 0 means don't sample
	ci.bipolar = bipolar;
        ci.id = id0;
	channels[ichan] = ci;

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

	sinfo.convSlopes[ivar] = 20.0 / 65535 / channels[ichan].gain * corSlope;
	if (channels[ichan].bipolar) 
	    sinfo.convIntercepts[ivar] = corIntercept;
	else 
	    sinfo.convIntercepts[ivar] = corIntercept +
	    	10.0 / channels[ichan].gain * corSlope;
    }
}

void DSC_A2DSensor::fromDOMElement(
	const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    DSMSensor::fromDOMElement(node);

    const std::list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi;
    for (pi = params.begin(); pi != params.end(); ++pi) {
        const Parameter* param = *pi;
        const string& pname = param->getName();
        if (pname == "rate") {
                if (param->getLength() != 1)
                    throw n_u::InvalidParameterException(getName(),"parameter",
                        "bad rate parameter");
                setScanRate((int)param->getNumericValue(0));
        }
    }
}

