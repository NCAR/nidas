/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-04-22 10:12:41 -0600 (Sun, 22 Apr 2007) $

    $LastChangedRevision: 3836 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynld/A2DSensor.cc $

 ******************************************************************
*/

#include <nidas/dynld/A2DSensor.h>
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

A2DSensor::A2DSensor() :
    DSMSensor(),initialized(false),
    scanRate(0),badRawSamples(0),
    rtlinux(-1)
{
    setLatency(0.1);
}

A2DSensor::~A2DSensor()
{
    for (unsigned int i = 0; i < _samples.size(); i++) {
        delete [] _samples[i].convSlopes;
        delete [] _samples[i].convIntercepts;
    }
}

bool A2DSensor::isRTLinux() const
{
    if (rtlinux < 0)  {
        const string& dname = getDeviceName();
        unsigned int fs = dname.rfind('/');
        if (fs != string::npos && (fs + 3) < dname.length() &&
            dname.substr(fs+1,3) == "rtl")
                    rtlinux = 1;
        else rtlinux = 0;
    }
    return rtlinux == 1;
}

void A2DSensor::open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException)
{
    DSMSensor::open(flags);
    init();
    config();
}

void A2DSensor::config()
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException)
{

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
	cerr << "chan=" << chan << " rate=" << cfg.scanRate <<
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

        ioctl(NIDAS_A2D_ADD_FILTER, &cfg,
            sizeof(struct nidas_a2d_filter_config));
    }
}


void A2DSensor::close() throw(n_u::IOException)
{
    DSMSensor::close();
}

void A2DSensor::init() throw(n_u::InvalidParameterException)
{
    if (initialized) return;
    initialized = true;
}

bool A2DSensor::process(const Sample* insamp,list<const Sample*>& results) throw()
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

    unsigned int sindex = 0;
    // if more than one sample, the first value is an index
    if (_samples.size() != 1 || nvalues == _samples[0].nvars + 1) {
        int index = *sp++;
        if (index < 0 || index >= (signed) _samples.size()) {
            badRawSamples++;
            return false;
        }
        sindex = index;
        nvalues--;
    }

    struct sample_info* sinfo = &_samples[sindex];
    SampleTag* stag = sinfo->stag;
    const vector<const Variable*>& vars = stag->getVariables();

    SampleT<float>* osamp = getSample<float>(sinfo->nvars);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(stag->getId());
    float *fp = osamp->getDataPtr();

    unsigned int ival;
    for (ival = 0; ival < std::min(nvalues,sinfo->nvars); ival++,fp++) {
	short sval = *sp++;
	if (sval == -32768 || sval == 32767) {
            *fp = floatNAN;
            continue;
        }

	float volts = sinfo->convIntercepts[ival] +
            sinfo->convSlopes[ival] * sval;
        const Variable* var = vars[ival];
        if (volts < var->getMinValue() || volts > var->getMaxValue()) 
            *fp = floatNAN;
        else {
            VariableConverter* conv = var->getConverter();
            if (conv) *fp = conv->convert(osamp->getTimeTag(),volts);
            else *fp = volts;
        }
    }

    for ( ; ival < sinfo->nvars; ival++) *fp++ = floatNAN;
    results.push_back(osamp);

    return true;
}

void A2DSensor::addSampleTag(SampleTag* tag)
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
                        "bad numpoints parameter");
                boxcarNpts = (int)param->getNumericValue(0);
        }
    }

    int sindex = _samples.size();       // sample index, 0,1,...

    const vector<const Variable*>& vars = tag->getVariables();

    sample_info sinfo;
    memset(&sinfo,0,sizeof(sinfo));
    sinfo.stag = tag;
    sinfo.index = sindex;
    sinfo.rate = rate;
    sinfo.filterType = filterType;
    sinfo.boxcarNpts = boxcarNpts;
    sinfo.nvars = vars.size();
    sinfo.convSlopes = new float[sinfo.nvars];
    sinfo.convIntercepts = new float[sinfo.nvars];
    for (unsigned int i = 0; i < sinfo.nvars; i++) {
        sinfo.convSlopes[i] = 1.0;
        sinfo.convIntercepts[i] = 0.0;
    }

    _samples.push_back(sinfo);
}

void A2DSensor::fromDOMElement(
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

