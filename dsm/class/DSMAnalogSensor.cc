/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

/* Some notes about blocking A2D data.
 * Suppose we support these rates of sampling:
 * 10K, 5K, 1K, 500, 100, 50,10,1 samples/sec
 *
 * Driver sends blocks of data at some rate, say 100/sec:
 * So a block would have:
 *    100 samples of each 10K/s channel
 *     50 samples of each 5K/s channel
 *     10 samples of each 1K/s channel
 *      5 samples of each 500/s channel
 *      1 samples of each 100/s channel
 * Every other block would have a sample from each 50/s channel
 * Every 1/10th block would have a sample from each 10/s channel
 * Every 1/100th block would have a sample from each 1/s channel
 *
 * One should be able to determine which samples are present
 * from the length of the data block, basically
 * step through the above table until counter==0.
 *
 * What are the time-tags of the samples? 
 * The block will have a time tag.
 * The timetags of the 10K data:
 *           t[i] = tblock - (99 - i) * .0001s , for i=0:99
 * Seems tricky to do the all of the above if the driver is doing FIR
 * filtering to achieve the given rates.
 */

#include <DSMAnalogSensor.h>
#include <a2d_driver.h>

#include <atdUtil/Logger.h>

#include <math.h>

#include <iostream>


using namespace dsm;
using namespace std;

CREATOR_ENTRY_POINT(DSMAnalogSensor)

DSMAnalogSensor::DSMAnalogSensor() :
    RTL_DSMSensor(),initialized(false),
    sampleIndices(0),subSampleIndices(0),convSlope(0),convIntercept(0),
    endTimes(0),
    deltatUsec(0),nSamplePerRawSample(0),
    outsamples(0),latency(0.1)
{
}

DSMAnalogSensor::~DSMAnalogSensor()
{
    delete [] sampleIndices;
    delete [] subSampleIndices;
    delete [] convSlope;
    delete [] convIntercept;
    delete [] endTimes;
    delete [] deltatUsec;
    if (outsamples) {
	for (unsigned int i = 0; i < rateVec.size(); i++)
	    if (outsamples[i]) outsamples[i]->freeReference();
	delete [] outsamples;
    }
}

void DSMAnalogSensor::open(int flags) throw(atdUtil::IOException)
{
    init();

    A2D_SET a2d;
    unsigned int chan;
    int maxrate = 0;
    for(chan = 0; chan < channels.size(); chan++)
    {
	a2d.Hz[chan] = channels[chan].rateSetting;
	a2d.gain[chan] = channels[chan].gainSetting;
	a2d.offset[chan] = !channels[chan].bipolar;	// flip logic
	if (a2d.Hz[chan] > maxrate) maxrate = a2d.Hz[chan];
	cerr << "chan=" << chan << " Hz=" << a2d.Hz[chan] <<
		" gain=" << a2d.gain[chan] << 
		" offset=" << a2d.offset[chan] << endl;
    }
    for( ; chan < MAXA2DS; chan++) {
	a2d.Hz[chan] = 0;
	a2d.gain[chan] = 0;
	a2d.offset[chan] = 0;
    }

    a2d.latencyUsecs = (int)(USECS_PER_SEC * getLatency());
    if (a2d.latencyUsecs == 0) a2d.latencyUsecs = USECS_PER_SEC / 10;

    ostringstream ost;
    if (maxrate >= 1000)
	ost << "filters/fir" << maxrate/1000. << "KHz.cfg";
    else
	ost << "filters/fir" << maxrate << "Hz.cfg";
    string filtername = ost.str();

    int nexpect = (signed)sizeof(a2d.filter)/sizeof(a2d.filter[0]);
    readFilterFile(filtername,a2d.filter,nexpect);

    cerr << "doing A2D_SET_IOCTL" << endl;
    ioctl(A2D_SET_IOCTL, &a2d, sizeof(A2D_SET));

    cerr << "doing A2D_RUN_IOCTL" << endl;
    ioctl(A2D_RUN_IOCTL,(const void*)0,0);

    RTL_DSMSensor::open(flags);
}

void DSMAnalogSensor::close() throw(atdUtil::IOException)
{
    cerr << "doing A2D_STOP_IOCTL" << endl;
    ioctl(A2D_STOP_IOCTL,(const void*)0,0);
    RTL_DSMSensor::close();
}

int DSMAnalogSensor::readFilterFile(const string& name,unsigned short* coefs,int nexpect)
{
    FILE* fp;
    atdUtil::Logger::getInstance()->log(LOG_NOTICE,"opening: %s",name.c_str());
    if((fp = fopen(name.c_str(), "r")) == NULL)
        throw atdUtil::IOException(name,"open",errno);

    int ncoef;
    for(ncoef = 0; ; ) {
	unsigned short val;
	int n = fscanf(fp, "%4hx", &val);
	if (ferror(fp)) {
	    fclose(fp);
	    throw atdUtil::IOException(name,"fscanf",errno);
	}
	if (feof(fp)) break;
	if (n != 1) {
	    if ((n = getc(fp)) != '#') {
		fclose(fp);
	    	throw atdUtil::IOException(name,"fscanf",
			string("bad input character: \'") +
			string((char)n,1) + "\'");
	    }
	    fscanf(fp,"%*[^\n]");	// skip to newline
	}
	else {
	    if (ncoef < nexpect) coefs[ncoef] = val;
	    ncoef++;
	}
    }
    fclose(fp);
    if (ncoef != nexpect) {
        ostringstream ost;
	ost << "wrong number of filter coefficients, expected: " <<
		nexpect << ", got: " << ncoef;
	throw atdUtil::IOException(name,"fscanf",ost.str());
    }
    return ncoef;
}

int DSMAnalogSensor::rateSetting(float rate)
	throw(atdUtil::InvalidParameterException)
{
    // todo: screen invalid rates
    return (int)rint(rate);
}

int DSMAnalogSensor::gainSetting(float gain)
	throw(atdUtil::InvalidParameterException)
{
    // todo: screen invalid gains
    return (int)rint(gain * 10.0);
}

void DSMAnalogSensor::init() throw(atdUtil::InvalidParameterException)
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
	 * A2D chip converts 0:4 V to -32767:32767
	 * 1. Input voltages are first converted by a F/256 converter
	 *     where F is the gainFactor, F=gain*10.
	 * 1.a Then inputs go through a *5.11 multiplier.
	 *    These two steps combined are a  C=gain*0.2
	 *	(actually gain*0.019961)
	 * 2. Then either a 2(bipolar) or 4(unipolar) volt offset is removed.
	 * 3. Then the voltage is inverted.
	 * 4. Converted to counts
	 * 
	 * Example: -10:10 V input, gain=1,bipolar=true
	 *	F=gain*10=10,  C=0.2,  offset=2
	 *	-10:10 -> -2:2 -> -4:0 -> 4:0 -> 32767:-32767
	 *
	 * The driver inverts the counts before passing them
	 * back to user space, so for purposes here, we can
	 * pretend that the A2D converts -4:0 volts to -32767:32767 counts
	 *
	 * For bipolar=T 
	 *	cnts = ((V * gain * 0.2) - 2) * 65536 / 4 + 32767 =
	 *		V * gain * 0.05 * 65536 + 0
	 *	So:   V = cnts * 20 / 65536 / gain
	 * For bipolar=F 
	 *	cnts = ((V * gain * 0.2) - 4) * 65536 / 4 + 32767 =
	 *		V * gain * 0.05 * 65536 - 32767
	 *	So:   V = (cnts + 32767) * 20 / 65536 / gain
	 *              = cnts * 20 / 65536 / gain + 10. / gain
	 *		= cnts * 20 / 65536 / gain + offset
	 *	where offset = 10/gain.
	 *
	 * corSlope and corIntercept are the slope and intercept
	 * of an A2D calibration, where
	 *    Vcorr = Vuncorr * corSlope + corIntercept
	 *
	 * Note that Vcorr is the Y (independent) variable. This is
	 * because the A2D calibration is done in a similar
	 * way to normal sensor calibration, where Y are the
	 * set points from an input standard, and X is the measured
	 * value.
	 *
	 *    Vcorr = Vuncorr * corSlope + corIntercept
	 *	    = (cnts * 20 / 65536 / gain + offset) * corSlope +
	 *			corIntercept
	 *	    = cnts * 20 / 65536 / gain * corSlope +
	 *		offset * corSlope + corIntercept
	 */

	convSlope[ivar] = 20.0 / 65536 / channels[ichan].gain * corSlopes[ichan];
	if (channels[ichan].bipolar) 
	    convIntercept[ivar] = corIntercepts[ichan];
	else 
	    convIntercept[ivar] = corIntercepts[ichan] +
	    	10.0 / channels[ichan].gain * corSlopes[ichan];
    }

    unsigned int nsamples = rateVec.size();

    deltatUsec = new int[nsamples];
    endTimes = new dsm_time_t[nsamples];

    float maxRate = 0.0;
    int imax = 0;
    for (unsigned int i = 0; i < rateVec.size(); i++) {
	if (rateVec[i] > maxRate) {
	    maxRate = rateVec[i];
	    imax = i;
	}
	deltatUsec[i] = (int)rint(USECS_PER_SEC / rateVec[i]);
	endTimes[i] = 0;
    }
    minDeltatUsec = deltatUsec[imax];

#define REPORTING_RATE 100
    if (maxRate >= REPORTING_RATE) {
	assert(fmod(maxRate,REPORTING_RATE) == 0.0);
        nSamplePerRawSample = (int)(maxRate / REPORTING_RATE);
    }
    else nSamplePerRawSample = 1;

#ifdef DEBUG
    cerr << "maxRate=" << maxRate <<
    	" nSamplePerRawSample=" << nSamplePerRawSample << endl;
#endif

    outsamples = new SampleT<float>*[nsamples];
    for (unsigned int i = 0; i < nsamples; i++) outsamples[i] = 0;
}

void DSMAnalogSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    A2D_STATUS stat;
    try {
	ioctl(A2D_STATUS_IOCTL,&stat,sizeof(stat));

	ostr << "<td align=left>" <<
		"#full=" << stat.fifofullctr <<
		", #3/4=" << stat.fifo34ctr <<
		", #1/2=" << stat.fifo24ctr <<
		", #1/4=" << stat.fifo14ctr <<
		", #0/4=" << stat.fifoemptyctr <<
		"</td>" << endl;
    }
    catch(const atdUtil::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}

bool DSMAnalogSensor::process(const Sample* insamp,list<const Sample*>& result) throw()
{

// #define DEBUG
#ifdef DEBUG
    static size_t debugcntr = 0;
#endif
    dsm_time_t tt = insamp->getTimeTag();

    // pointer to raw A2D counts
    const signed short* sp = (const signed short*) insamp->getConstVoidDataPtr();

    // number of data values in this raw sample.
    unsigned int nvalues = insamp->getDataByteLength() / sizeof(short);

    // number of variables being sampled
    unsigned int nvariables = sampleIndexVec.size();

    // One raw sample from A2D contains multiple sweeps
    // of the A2D channels.
    assert((nvalues % nvariables) == 0);

    for (unsigned int ival = 0; ival < nvalues; ) {

	for (unsigned int ivalmod = 0;
		ivalmod < nvariables && ival < nvalues; ivalmod++,ival++) {
	    int isamp = sampleIndices[ivalmod];
	    int sampIndex = subSampleIndices[ivalmod];

#ifdef DEBUG
	    if (!(debugcntr % 100))
	    cerr << " nvariables=" << nvariables << " nvalues=" << nvalues <<
		    " ival=" << ival << " ivalmod=" << ivalmod <<
		    " isamp=" << isamp << " sampIndex=" << sampIndex << endl;
#endif
	    SampleT<float>* osamp = outsamples[isamp];

#ifdef DEBUG
	    if (!(debugcntr % 100))
	    cerr << "tt=" << tt << " endTimes=" << endTimes[isamp] << endl;
#endif
	    if (tt > endTimes[isamp]) {
		if (osamp) {
		    osamp->setTimeTag(endTimes[isamp] - deltatUsec[isamp]/2);
#ifdef DEBUG
	    if (!(debugcntr % 100)) {
		    cerr << "tt=" << osamp->getTimeTag() <<
			    " len=" << osamp->getDataLength() << " data:";
		    for (unsigned int j = 0; j < osamp->getDataLength(); j++)
			cerr << osamp->getDataPtr()[j] <<  ' ';
		    cerr << endl;
	    }
#endif
		    result.push_back(osamp);	// pass the sample on
		    endTimes[isamp] += deltatUsec[isamp];
		    if (tt > endTimes[isamp])
			endTimes[isamp] = timeCeiling(tt,deltatUsec[isamp]);
		}
		else
		    endTimes[isamp] = timeCeiling(tt,deltatUsec[isamp]);
#ifdef DEBUG
	    if (!(debugcntr % 100))
		cerr << "getSample, numVarsInSample[" << isamp << "]=" << numVarsInSample[isamp] << endl;
#endif
		osamp = getSample<float>(numVarsInSample[isamp]);
		outsamples[isamp] = osamp;
		osamp->setId(sampleIds[isamp]);
#ifdef DEBUG
	    if (!(debugcntr % 100))
		cerr << "osamp->getDataLength()=" << osamp->getDataLength() << endl;
#endif
		float *fp = osamp->getDataPtr();
		for (unsigned int j = 0; j < osamp->getDataLength(); j++)
		    fp[j] = floatNAN;
	    }

	    signed short sval = sp[ival];
	    float volts;
	    if (sval == -32768) volts = floatNAN;
	    else volts = convIntercept[ivalmod] + convSlope[ivalmod] * sval;
#ifdef DEBUG
	    if (!(debugcntr % 100))
	    cerr << "ivalmod=" << ivalmod << " ival=" << ival <<
		" sval=" << sval << " volts=" << volts <<
		" convIntercept[ivalmod]=" << convIntercept[ivalmod] <<
		" convSlope[ivalmod]=" << convSlope[ivalmod] <<
		" outindex=" << sampIndex << endl;
#endif
	    osamp->getDataPtr()[sampIndex] = volts;
	}
	tt += minDeltatUsec;

    }
#ifdef DEBUG
    debugcntr++;
#endif
    return true;
}

void DSMAnalogSensor::addSampleTag(SampleTag* tag)
        throw(atdUtil::InvalidParameterException)
{

    DSMSensor::addSampleTag(tag);

    float rate = tag->getRate();
    int ratesetting = rateSetting(rate);

    int nsample = rateVec.size();	// which sample are we, from 0

    rateVec.push_back(rate);	// rates can be repeated
    sampleIds.push_back(tag->getId());

    const vector<const Variable*>& vars = tag->getVariables();
    numVarsInSample.push_back(vars.size());

    vector<const Variable*>::const_iterator vi;
    int ivar = 0;
    for (vi = vars.begin(); vi != vars.end(); ++vi,ivar++) {

	const Variable* var = *vi;

	float gain = 0.0;
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
			
		gain = param->getNumericValue(0);
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
	if (ichan >= MAXA2DS) {
	    ostringstream ost;
	    ost << MAXA2DS;
	    throw atdUtil::InvalidParameterException(getName(),"variable",
		string("cannot sample more than ") + ost.str() +
		    string(" channels"));
	}

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

	if (gain == 0.0) {	// gain of 0 means don't sample, set rate=0 for driver
	    ci.rate =  0;
	    ci.rateSetting = 0;
	}
	else {
	    ci.rate = rate;
	    ci.rateSetting = ratesetting;
	}

	ci.gain = gain;
	ci.gainSetting = gainSetting(gain);
	ci.bipolar = bipolar;
	channels[ichan] = ci;

	channelNums.push_back(ichan);

	if (sortedChannelNums.find(ichan) != sortedChannelNums.end()) {
	    ostringstream ost;
	    ost << MAXA2DS;
	    throw atdUtil::InvalidParameterException(getName(),"variable",
		string("multiple variables for A2D channel ") + ost.str());
	}
	sortedChannelNums.insert(ichan);

        sampleIndexVec.push_back(nsample);	// which sample this variable belongs to
	subSampleIndexVec.push_back(ivar);	// which variable within sample
    }
}

