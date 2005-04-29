/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
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
    endTimes(0),baseTimes(0),nsamps(0),
    deltatDouble(0),deltatInt(0),deltatEvenMsec(0),
    nSamplePerRawSample(0),
    outsamples(0),
    floatNAN(nanf(""))
{
}

DSMAnalogSensor::~DSMAnalogSensor()
{
    delete [] sampleIndices;
    delete [] subSampleIndices;
    delete [] convSlope;
    delete [] convIntercept;
    delete [] endTimes;
    delete [] baseTimes;
    delete [] nsamps;
    delete [] deltatDouble;
    delete [] deltatInt;
    delete [] deltatEvenMsec;
    if (outsamples) {
	for (unsigned int i = 0; i < rateVec.size(); i++)
	    if (outsamples[i]) outsamples[i]->freeReference();
	delete [] outsamples;
    }
}

void DSMAnalogSensor::open(int flags) throw(atdUtil::IOException)
{
    init();
    RTL_DSMSensor::open(flags);

    A2D_SET a2d;
    unsigned int chan;
    int maxrate = 0;
    for(chan = 0; chan < channels.size(); chan++)
    {
	a2d.Hz[chan] = channels[chan].rateSetting;
	a2d.gain[chan] = channels[chan].gainSetting;
	a2d.offset[chan] = channels[chan].bipolar;
	a2d.status[chan] = 0; 
	a2d.calset[chan] = 0;
	if (a2d.Hz[chan] > maxrate) maxrate = a2d.Hz[chan];

	if(chan == 0)a2d.ptr[chan] = 0;
	else 
	{
		a2d.ptr[chan] = a2d.ptr[chan-1] + a2d.Hz[chan-1];
	}
	a2d.ctr[chan] = 0;	// Reset counters	
	if(a2d.Hz[chan] != 0)a2d.norm[chan] = 
				(float)a2d.Hz[chan]/float(A2D_MAX_RATE);
    }
    for( ; chan < MAXA2DS; chan++) {
	a2d.Hz[chan] = 0;
	a2d.gain[chan] = 0;
	a2d.offset[chan] = 0;
	a2d.status[chan] = 0; 
	a2d.calset[chan] = 0;
    }

    ostringstream ost;
    if (maxrate >= 1000)
	ost << "filters/fir" << maxrate/1000. << "KHz.cfg";
    else
	ost << "filters/fir" << maxrate << "Hz.cfg";
    string filtername = ost.str();

    FILE* fp;
    atdUtil::Logger::getInstance()->log(LOG_INFO,"opening: %s",filtername.c_str());
    if((fp = fopen(filtername.c_str(), "r")) == NULL)
        throw atdUtil::IOException(filtername,"open",errno);

    unsigned int ncoef;
    for(ncoef = 0; ncoef < sizeof(a2d.filter)/sizeof(a2d.filter[0]); ncoef++)
    {
	int n = fscanf(fp, "%4hx", a2d.filter + ncoef);
	if (ferror(fp))
	    throw atdUtil::IOException(filtername,"fscanf",errno);
	if (feof(fp)) break;
	if (n != 1)
	    throw atdUtil::IOException(filtername,"fscanf","bad input");
    }
    fclose(fp);
    cerr << "ncoef=" << ncoef << " expected=" <<
		sizeof(a2d.filter)/sizeof(a2d.filter[0]) << endl;
    assert(ncoef == sizeof(a2d.filter)/sizeof(a2d.filter[0]));

    cerr << "doing A2D_SET_IOCTL" << endl;
    ioctl(A2D_SET_IOCTL, &a2d, sizeof(A2D_SET));

    cerr << "doing A2D_RUN_IOCTL" << endl;
    ioctl(A2D_RUN_IOCTL,(const void*)0,0);
}

void DSMAnalogSensor::close() throw(atdUtil::IOException)
{
    cerr << "doing A2D_STOP_IOCTL" << endl;
    ioctl(A2D_STOP_IOCTL,(const void*)0,0);
    RTL_DSMSensor::close();
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

void DSMAnalogSensor::init() throw()
{
    assert(!initialized);
    initialized = true;

    unsigned int nvariables = sampleIndexVec.size();

    sampleIndices = new int[nvariables];
    copy(sampleIndexVec.begin(),sampleIndexVec.end(),sampleIndices);

    subSampleIndices = new int[nvariables];
    copy(subSampleIndexVec.begin(),subSampleIndexVec.end(),subSampleIndices);

    convSlope = new float[nvariables];
    convIntercept = new float[nvariables];
    for (unsigned int i = 0; i < nvariables; i++) {
		if (channels[i].bipolar) {
			convSlope[i] = 10.0 / 32768 / channels[i].gain;
			convIntercept[i] = 0.0;
		}
		else {
			convSlope[i] = 10.0 / 65536 / channels[i].gain;
			convIntercept[i] = 5.0;
		}
    }

    unsigned int nsamples = rateVec.size();

    deltatDouble = new double[nsamples];
    deltatInt = new int[nsamples];
    deltatEvenMsec = new bool[nsamples];
    endTimes = new dsm_time_t[nsamples];
    baseTimes = new dsm_time_t[nsamples];
    nsamps = new size_t[nsamples];

    float maxRate = 0.0;
    int imax = 0;
    for (unsigned int i = 0; i < rateVec.size(); i++) {
	if (rateVec[i] > maxRate) {
	    maxRate = rateVec[i];
	    imax = i;
	}
	deltatDouble[i] = 1000.0 / rateVec[i];
	deltatInt[i] = (int)rint(1000 / rateVec[i]);
	deltatEvenMsec[i] = (deltatInt[i] == deltatDouble[i]);
	endTimes[i] = 0;
	baseTimes[i] = 0;
    }
    minDeltatDouble = deltatDouble[imax];
    minDeltatInt = deltatInt[imax];
    minDeltatEvenMsec = deltatEvenMsec[imax];

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
		", #1/2=" << stat.fifo12ctr <<
		", #1/4=" << stat.fifo14ctr <<
		", #0/4=" << stat.fifo0ctr <<
		", #rtlFifoErrors=" << stat.rtlFifoWriteErrors <<
		"</td>" << endl;
    }
    catch(const atdUtil::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}

bool DSMAnalogSensor::process(const Sample* isamp,list<const Sample*>& result) throw()
{

    dsm_time_t tt = isamp->getTimeTag();
    dsm_time_t tt0 = tt;

    const signed short* sp = (const signed short*) isamp->getConstVoidDataPtr();

    unsigned int nvalues = isamp->getDataLength() / sizeof(short);
    unsigned int nvariables = sampleIndexVec.size();
    // assert(nvariables * nSamplePerRawSample == nvalues);
    unsigned int nsampsInRawSample = nvalues / nvariables;

    unsigned int ival = 0;
    for (unsigned int isamp = 0; isamp < nsampsInRawSample; ) {

#ifdef DEBUG
	cerr << "isamp=" << isamp << " nsampsInRawSample=" << nsampsInRawSample << endl;
#endif
	for (unsigned int ivar = 0; ivar < nvariables && ival < nvalues; ivar++,ival++) {
#ifdef DEBUG
	cerr << "ivar=" << ivar << " nvariables=" << nvariables <<
		" ival=" << ival << " nvalues=" << nvalues << endl;
#endif
	    int sampIndex = sampleIndices[ivar];
#ifdef DEBUG
	    cerr << "sampIndex=" << sampIndex << endl;
#endif
	    SampleT<float>* osamp = outsamples[sampIndex];

#ifdef DEBUG
	    cerr << "tt=" << tt << " endTimes=" << endTimes[sampIndex] << endl;
#endif
	    if (tt > endTimes[sampIndex]) {
		if (osamp) {
		    osamp->setTimeTag(endTimes[sampIndex] - deltatInt[sampIndex]/2);
#ifdef DEBUG
		    cerr << "tt=" << osamp->getTimeTag() <<
			    " len=" << osamp->getDataLength() << " data:";
		    for (unsigned int j = 0; j < osamp->getDataLength(); j++)
			cerr << osamp->getDataPtr()[j] <<  ' ';
		    cerr << endl;
#endif
		    result.push_back(osamp);	// pass the sample on
		    if (deltatEvenMsec[sampIndex]) {
			endTimes[sampIndex] += deltatInt[sampIndex];
			if (tt > endTimes[sampIndex])
			    endTimes[sampIndex] = timeCeiling(tt,deltatInt[sampIndex]);
		    }
		    else {
			endTimes[sampIndex] =
			    baseTimes[sampIndex] +
			    (long long) rint(++nsamps[sampIndex] *
			    	deltatDouble[sampIndex]);
			if (tt > endTimes[sampIndex]) {
			    int nt = ceil((tt - endTimes[sampIndex]) /
			    	deltatDouble[sampIndex]);
			    nsamps[sampIndex] += nt;
			    endTimes[sampIndex] +=
			    	(long long)rint(nt * deltatDouble[sampIndex]);
			}
		    }
		}
		else {
		    baseTimes[sampIndex] = timeCeiling(tt,deltatInt[sampIndex]);
		    endTimes[sampIndex] = baseTimes[sampIndex];
		    nsamps[sampIndex] = 0;
		}
#ifdef DEBUG
		cerr << "getSample, numVarsInSample[" << sampIndex << "]=" << numVarsInSample[sampIndex] << endl;
#endif
		osamp = getSample<float>(numVarsInSample[sampIndex]);
		outsamples[sampIndex] = osamp;
		osamp->setId(sampleIds[sampIndex]);
#ifdef DEBUG
		cerr << "osamp->getDataLength()=" << osamp->getDataLength() << endl;
#endif
		float *fp = osamp->getDataPtr();
		for (unsigned int j = 0; j < osamp->getDataLength(); j++)
		    fp[j] = floatNAN;
	    }

	    signed short sval = sp[ival];
	    float volts = convIntercept[ivar] + convSlope[ivar] * sval;
#ifdef DEBUG
	    cerr << "ivar=" << ivar << " ival=" << ival <<
	    	" sval=" << sval << " volts=" << volts <<
		" outindex=" << subSampleIndices[ivar] << endl;
#endif
	    osamp->getDataPtr()[subSampleIndices[ivar]] = volts;
	}

	isamp++;
	if (minDeltatEvenMsec) tt += minDeltatInt;
	else tt = tt0 + (long long) rint(minDeltatDouble * isamp);
    }
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

	if (sampleIndexVec.size() == MAXA2DS) {
	    ostringstream ost;
	    ost << MAXA2DS;
	    throw atdUtil::InvalidParameterException(getName(),"variable",
		string("cannot sample more than ") + ost.str() +
		    string(" variables"));
	}

        sampleIndexVec.push_back(nsample);
	subSampleIndexVec.push_back(ivar);

	float gain = 1.0;
	bool bipolar = true;

	const std::list<const Parameter*>& params = var->getParameters();
	list<const Parameter*>::const_iterator pi;
	for (pi = params.begin(); pi != params.end(); ++pi) {
	    const Parameter* param = *pi;
	    const string& pname = param->getName();
	    if (!pname.compare("gain")) {
		if (param->getLength() != 1)
		    throw atdUtil::InvalidParameterException(getName(),
		    	"parameter gain","no gain value");
			
		gain = param->getNumericValue(0);
	    }
	    else if (!pname.compare("bipolar")) {
		if (param->getLength() != 1)
		    throw atdUtil::InvalidParameterException(getName(),
		    	"parameter gain","no gain value");
		bipolar = param->getNumericValue(0) != 0;
	    }

	}

	struct chan_info ci;
	ci.rate = rate;
	ci.rateSetting = ratesetting;
	ci.gain = gain;
	ci.gainSetting = gainSetting(gain);
	ci.bipolar = bipolar;
	channels.push_back(ci);
    }
}

