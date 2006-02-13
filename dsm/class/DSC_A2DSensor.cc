/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <DSC_A2DSensor.h>
#include <RTL_IODevice.h>
#include <UnixIODevice.h>
#include <dsc_a2d.h>

#include <atdUtil/Logger.h>

#include <math.h>

#include <iostream>


using namespace dsm;
using namespace std;

CREATOR_FUNCTION(DSC_A2DSensor)

DSC_A2DSensor::DSC_A2DSensor() :
    DSMSensor(),initialized(false),
    sampleIndices(0),subSampleIndices(0),
    convSlope(0),convIntercept(0),
    sampleTimes(0),deltatUsec(0),
    outsamples(0),latency(0.1),badRawSamples(0),
    rtlinux(-1)
{
}

DSC_A2DSensor::~DSC_A2DSensor()
{
    delete [] sampleIndices;
    delete [] subSampleIndices;
    delete [] convSlope;
    delete [] convIntercept;
    delete [] sampleTimes;
    delete [] deltatUsec;
    if (outsamples) {
	for (unsigned int i = 0; i < rateVec.size(); i++)
	    if (outsamples[i]) outsamples[i]->freeReference();
	delete [] outsamples;
    }
}

bool DSC_A2DSensor::isRTLinux() const
{
    if (rtlinux < 0)  {
        const string& dname = getDeviceName();
        unsigned int fs = dname.rfind('/');
        if (fs != string::npos && (fs + 10) < dname.length() &&
            !dname.substr(fs+1,10).compare("rtldsc_a2d"))
                    rtlinux = 1;
        else rtlinux = 0;
    }
    return rtlinux == 1;
}

IODevice* DSC_A2DSensor::buildIODevice() throw(atdUtil::IOException)
{
    if (isRTLinux()) return new RTL_IODevice();
    else return new UnixIODevice();
}

SampleScanner* DSC_A2DSensor::buildSampleScanner()
{
    return new SampleScanner();
}

void DSC_A2DSensor::open(int flags) throw(atdUtil::IOException)
{
    DSMSensor::open(flags);

    init();

    int nchans;
    ioctl(DSC_GET_NCHAN,&nchans,sizeof(int));

    struct DSC_Config cfg;
    unsigned int chan;
    for(chan = 0; chan < channels.size(); chan++)
    {
	if (chan >= (unsigned)nchans) {
	    ostringstream ost;
	    ost << "max channel number is " << nchans;
	    throw atdUtil::IOException(getName(),"open",ost.str());
	}
	cfg.rate[chan] = channels[chan].rate;
	cfg.gain[chan] = channels[chan].gain;
	cfg.bipolar[chan] = channels[chan].bipolar;

#ifdef DEBUG
	cerr << "chan=" << chan << " rate=" << cfg.rate[chan] <<
		" gain=" << cfg.gain[chan] << 
		" bipolar=" << cfg.bipolar[chan] << endl;
#endif
    }
    for( ; chan < NUM_DSC_CHANNELS; chan++) {
	cfg.rate[chan] = 0;
	cfg.gain[chan] = 0;
	cfg.bipolar[chan] = false;
    }

    cfg.latencyUsecs = (int)(USECS_PER_SEC * getLatency());
    if (cfg.latencyUsecs == 0) cfg.latencyUsecs = USECS_PER_SEC / 10;

    // cerr << "doing DSC_CONFIG" << endl;
    ioctl(DSC_CONFIG, &cfg, sizeof(struct DSC_Config));

    // cerr << "doing DSC_START" << endl;
    ioctl(DSC_START,0,0);

}

void DSC_A2DSensor::close() throw(atdUtil::IOException)
{
    cerr << "doing DSC_STOP" << endl;
    ioctl(DSC_STOP,0,0);
    DSMSensor::close();
}

void DSC_A2DSensor::init() throw(atdUtil::InvalidParameterException)
{
    if (initialized) return;
    initialized = true;

    // number of variables being sampled
    unsigned int nvariables = sampleIndexVec.size();

    sampleIndices = new int[nvariables];
    subSampleIndices = new int[nvariables];
    convSlope = new float[nvariables];
    convIntercept = new float[nvariables];

    set<int>::const_iterator si;
    int ivar = 0;
    for (si = sortedChannelNums.begin(); si != sortedChannelNums.end();
    	si++,ivar++) {
	int ichan = *si;	// a2d channel number
	size_t i;		// which requested variable is ichan
        for (i = 0; i < channelNums.size(); i++)
	    if (channelNums[i] == ichan) break;
	assert(i < channelNums.size());	// it must be here somewhere

	// In channel index order, which output samples a channel is
	// to be written to.
	sampleIndices[ivar] = sampleIndexVec[i];
	// In channel index order, which index into an output sample
	subSampleIndices[ivar] = subSampleIndexVec[i];

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

	convSlope[ivar] = 20.0 / 65535 / channels[ichan].gain * corSlopes[ichan];
	if (channels[ichan].bipolar) 
	    convIntercept[ivar] = corIntercepts[ichan];
	else 
	    convIntercept[ivar] = corIntercepts[ichan] +
	    	10.0 / channels[ichan].gain * corSlopes[ichan];
#ifdef DEBUG
	cerr << "ivar=" << ivar << " sampleIndices[ivar]=" << sampleIndices[ivar] <<
		" rate=" << rateVec[sampleIndices[ivar]] << endl;
#endif
    }

    unsigned int nsamples = rateVec.size();

    deltatUsec = new int[nsamples];
    sampleTimes = new dsm_time_t[nsamples];

    for (unsigned int i = 0; i < rateVec.size(); i++) {
	deltatUsec[i] = (int)(USECS_PER_SEC / rateVec[i]);
	sampleTimes[i] = 0;
    }

    outsamples = new SampleT<float>*[nsamples];
    for (unsigned int i = 0; i < nsamples; i++) outsamples[i] = 0;
}

void DSC_A2DSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    struct DSC_Status stat;
    try {
	ioctl(DSC_STATUS,&stat,sizeof(stat));
	ostr << "<td align=left>";
	ostr << "FIFO: over=" << stat.fifoOverflows << 
		", under=" << stat.fifoUnderflows <<
		", not empty=" << stat.fifoNotEmpty <<
		", lostSamples=" << stat.missedSamples;
	ostr << "</td>" << endl;
    }
    catch(const atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: printStatus: %s",getName().c_str(),
	    ioe.what());
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}

bool DSC_A2DSensor::process(const Sample* insamp,list<const Sample*>& result) throw()
{

// #define DEBUG
#ifdef DEBUG
    static size_t debugcntr = 0;
#endif
    dsm_time_t tt = insamp->getTimeTag();

    // pointer to raw A2D counts
    const short* sp = (const short*) insamp->getConstVoidDataPtr();

    // raw data are shorts
    assert((insamp->getDataByteLength() % sizeof(short)) == 0);

    // number of data values in this raw sample.
    unsigned int nvalues = insamp->getDataByteLength() / sizeof(short);

    // number of variables being sampled
    unsigned int nvariables = sampleIndexVec.size();

    if (nvalues != nvariables) {
        if (!(badRawSamples++ % 1000)) 
		atdUtil::Logger::getInstance()->log(LOG_ERR,
			"A/D sample id %d (dsm=%d,sensor=%d): Expected %d raw values, got %d",
			insamp->getId(),GET_DSM_ID(insamp->getId()),GET_SHORT_ID(insamp->getId()),
			nvariables,nvalues);
	return false;
    }

#ifdef DEBUG
    bool debug = GET_DSM_ID(insamp->getId()) == 0;
#endif

    for (unsigned int ival = 0; ival < nvalues; ival++) {

	int isamp = sampleIndices[ival];
	int sampIndex = subSampleIndices[ival];

#ifdef DEBUG
	if (debug && !(debugcntr % 100))
	    cerr << " nvariables=" << nvariables << " nvalues=" << nvalues <<
		" ival=" << ival << 
		" isamp=" << isamp << " sampIndex=" << sampIndex << endl;
#endif
	SampleT<float>* osamp = outsamples[isamp];

#ifdef DEBUG
	if (debug && !(debugcntr % 100))
	    cerr << "tt=" << tt << " sampleTimes=" << sampleTimes[isamp] << endl;
#endif
	// send out last sample if time tag has incremented
	if (tt > sampleTimes[isamp]) {
	    if (osamp) {
		osamp->setTimeTag(sampleTimes[isamp]);
#ifdef DEBUG
		if (debug && !(debugcntr % 100)) {
		    cerr << "tt=" << osamp->getTimeTag() <<
			    " len=" << osamp->getDataLength() << " data:";
		    for (unsigned int j = 0; j < osamp->getDataLength(); j++)
			cerr << osamp->getDataPtr()[j] <<  ' ';
		    cerr << endl;
		}
#endif
		result.push_back(osamp);	// pass the sample on
		sampleTimes[isamp] += deltatUsec[isamp];
		if (tt > sampleTimes[isamp]) sampleTimes[isamp] = tt;
	    }
	    else
		sampleTimes[isamp] = tt;
#ifdef DEBUG
	    if (debug && !(debugcntr % 100))
		cerr << "getSample, numVarsInSample[" << isamp << "]=" << numVarsInSample[isamp] << endl;
#endif
	    osamp = getSample<float>(numVarsInSample[isamp]);
	    outsamples[isamp] = osamp;
	    osamp->setId(sampleIds[isamp]);
#ifdef DEBUG
	    if (debug && !(debugcntr % 100))
		cerr << "osamp->getDataLength()=" << osamp->getDataLength() << endl;
#endif
	    float *fp = osamp->getDataPtr();
	    for (unsigned int j = 0; j < osamp->getDataLength(); j++)
		fp[j] = floatNAN;
	}

	short sval = sp[ival];
	float volts;
	// if (sval == 32768) volts = floatNAN;
	volts = convIntercept[ival] + convSlope[ival] * sval;
#ifdef DEBUG
	if (debug && !(debugcntr % 100))
	    cerr << " ival=" << ival <<
		" sval=" << sval << " volts=" << volts <<
		" convIntercept[ival]=" << convIntercept[ival] <<
		" convSlope[ival]=" << convSlope[ival] <<
	    " outindex=" << sampIndex << endl;
#endif
	osamp->getDataPtr()[sampIndex] = volts;
    }
    
#ifdef DEBUG
    if (debug) debugcntr++;
#endif
    return true;
#undef DEBUG
}

void DSC_A2DSensor::addSampleTag(SampleTag* tag)
        throw(atdUtil::InvalidParameterException)
{

    DSMSensor::addSampleTag(tag);

    float frate = tag->getRate();
    if (fmod(frate,1.0) != 0.0) {
	ostringstream ost;
	ost << frate;
        throw atdUtil::InvalidParameterException(getName(),
		"rate must be an integer",ost.str());
    }

    int rate = (int)frate;

    int nsample = rateVec.size();	// which sample are we, from 0

    rateVec.push_back(rate);	// rates can be repeated
    sampleIds.push_back(tag->getId());

    const vector<const Variable*>& vars = tag->getVariables();
    numVarsInSample.push_back(vars.size());

    vector<const Variable*>::const_iterator vi;
    int ivar = 0;
    for (vi = vars.begin(); vi != vars.end(); ++vi,ivar++) {

	const Variable* var = *vi;

	float fgain = 0.0;
	bool bipolar = true;
	int ichan = channels.size();
	float corSlope = 1.0;
	float corIntercept = 0.0;

	const std::list<const Parameter*>& params = var->getParameters();
	list<const Parameter*>::const_iterator pi;
	for (pi = params.begin(); pi != params.end(); ++pi) {
	    const Parameter* param = *pi;
	    const string& pname = param->getName();
	    if (!pname.compare("gain")) {
		if (param->getLength() != 1)
		    throw atdUtil::InvalidParameterException(getName(),
		    	pname,"no value");
			
		fgain = param->getNumericValue(0);
	    }
	    else if (!pname.compare("bipolar")) {
		if (param->getLength() != 1)
		    throw atdUtil::InvalidParameterException(getName(),
		    	pname,"no value");
		bipolar = param->getNumericValue(0) != 0;
	    }
	    else if (!pname.compare("channel")) {
		if (param->getLength() != 1)
		    throw atdUtil::InvalidParameterException(getName(),pname,"no value");
		ichan = (int)param->getNumericValue(0);
	    }
	    else if (!pname.compare("corSlope")) {
		if (param->getLength() != 1)
		    throw atdUtil::InvalidParameterException(getName(),
		    	pname,"no value");
		corSlope = param->getNumericValue(0);
	    }
	    else if (!pname.compare("corIntercept")) {
		if (param->getLength() != 1)
		    throw atdUtil::InvalidParameterException(getName(),
		    	pname,"no value");
		corIntercept = param->getNumericValue(0);
	    }

	}
	if (ichan >= NUM_DSC_CHANNELS) {
	    ostringstream ost;
	    ost << NUM_DSC_CHANNELS;
	    throw atdUtil::InvalidParameterException(getName(),"variable",
		string("cannot sample more than ") + ost.str() +
		    string(" channels"));
	}

	if (fmod(fgain,1.0) != 0.0) {
	    ostringstream ost;
	    ost << fgain;
	    throw atdUtil::InvalidParameterException(getName(),
		    "gain must be an integer",ost.str());
	}
	int gain = (int)fgain;

	struct chan_info ci;
	memset(&ci,0,sizeof(ci));
	for (int i = channels.size(); i <= ichan; i++) channels.push_back(ci);

	for (int i = corSlopes.size(); i <= ichan; i++) {
	    corSlopes.push_back(1.0);
	    corIntercepts.push_back(0.0);
	}
	corSlopes[ichan] = corSlope;
	corIntercepts[ichan] = corIntercept;

	ci = channels[ichan];

	if (gain == 0) 	// gain of 0 means don't sample, set rate=0 for driver
	    ci.rate =  0;
	else ci.rate = rate;

	ci.gain = gain;
	ci.bipolar = bipolar;
	channels[ichan] = ci;

	channelNums.push_back(ichan);

	if (sortedChannelNums.find(ichan) != sortedChannelNums.end()) {
	    ostringstream ost;
	    ost << ichan;
	    throw atdUtil::InvalidParameterException(getName(),"variable",
		string("multiple variables for A2D channel ") + ost.str());
	}
	sortedChannelNums.insert(ichan);

        sampleIndexVec.push_back(nsample);	// which output sample this variable belongs to
#ifdef DEBUG
	cerr << "sampleIndexVec.size()=" << sampleIndexVec.size() << 
		" var=" << var->getName() << endl;
#endif
	subSampleIndexVec.push_back(ivar);	// which variable within sample
    }
}

