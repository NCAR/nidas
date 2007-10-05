/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/dynld/raf/DSMAnalogSensor.h>
#include <nidas/core/RTL_IODevice.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/linux/ncar_a2d.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>
#include <iomanip>

using namespace nidas::dynld::raf;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,DSMAnalogSensor)

DSMAnalogSensor::DSMAnalogSensor() :
    A2DSensor(),rtlinux(-1),
    _temperatureTag(0),_temperatureRate(IRIG_NUM_RATES),
    DEGC_PER_CNT(0.0625)
{
    setScanRate(500);   // lowest scan rate supported by card
    setLatency(0.1);
}

DSMAnalogSensor::~DSMAnalogSensor()
{
}

bool DSMAnalogSensor::isRTLinux() const
{
    if (rtlinux < 0)  {
        const string& dname = getDeviceName();
        string::size_type fs = dname.rfind('/');
        if (fs != string::npos && (fs + 6) < dname.length() &&
            dname.substr(fs+1,6) == "dsma2d")
                    rtlinux = 1;
        else rtlinux = 0;
    }
    n_u::Logger::getInstance()->log(LOG_NOTICE,"DSMAnalogSensor::isRTLinux(): %d",rtlinux);
    return rtlinux == 1;
}

IODevice* DSMAnalogSensor::buildIODevice() throw(n_u::IOException)
{
    if (isRTLinux()) return new RTL_IODevice();
    else return new UnixIODevice();
}

SampleScanner* DSMAnalogSensor::buildSampleScanner()
{
    return new SampleScanner();
}

void DSMAnalogSensor::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{

    DSMSensor::open(flags);
    init();

    A2D_SET a2d;
    unsigned int chan;
    for(chan = 0; chan < _channels.size(); chan++)
    {
	a2d.gain[chan] = _channels[chan].gainSetting;
	a2d.gainMul[chan] = _channels[chan].gainMul;
	a2d.gainDiv[chan] = _channels[chan].gainDiv;
	a2d.offset[chan] = !_channels[chan].bipolar;	// flip logic
	a2d.sampleIndex[chan] = _channels[chan].index;
        // for old driver
	a2d.Hz[chan] = 
            (_channels[chan].gainSetting == 0) ? 0 : getScanRate();
    }
    for( ; chan < NUM_NCAR_A2D_CHANNELS; chan++) {
	a2d.gain[chan] = 0;
	a2d.offset[chan] = 0;
	a2d.sampleIndex[chan] = -1;
	a2d.Hz[chan] = 0;
    }

    a2d.latencyUsecs = (int)(USECS_PER_SEC * getLatency());
    if (a2d.latencyUsecs == 0) a2d.latencyUsecs = USECS_PER_SEC / 10;

    ostringstream ost;
    if (getScanRate() >= 1000)
	ost << "/tmp/code/filters/fir" << getScanRate()/1000. << "KHz.cfg";
    else
	ost << "/tmp/code/filters/fir" << getScanRate() << "Hz.cfg";
    string filtername = ost.str();

    a2d.scanRate = getScanRate();

    int nexpect = (signed)sizeof(a2d.filter)/sizeof(a2d.filter[0]);
    readFilterFile(filtername,a2d.filter,nexpect);

    cerr << "doing A2D_SET_CONFIG" << endl;
    ioctl(A2D_SET_CONFIG, &a2d, sizeof(A2D_SET));

    for(unsigned int i = 0; i < _samples.size(); i++) {
        struct nidas_a2d_filter_config fcfg;

        fcfg.index = _samples[i].index;
	fcfg.rate = _samples[i].rate;
	fcfg.filterType = _samples[i].filterType;
	fcfg.boxcarNpts = _samples[i].boxcarNpts;

        cerr << "doing NIDAS_A2D_ADD_FILTER" << endl;
        ioctl(NIDAS_A2D_ADD_FILTER, &fcfg,
            sizeof(struct nidas_a2d_filter_config));
    }

    if (_temperatureRate != IRIG_NUM_RATES) {
        cerr << "Doing A2DTEMP_SET_RATE" << endl;
	ioctl(A2DTEMP_SET_RATE, &_temperatureRate, sizeof(_temperatureRate));
        cerr << "Done A2DTEMP_SET_RATE" << endl;
    }

    cerr << "doing A2D_RUN_IOCTL" << endl;
    ioctl(A2D_RUN, 0, 0);

}

void DSMAnalogSensor::close() throw(n_u::IOException)
{
    // cerr << "doing A2D_STOP_IOCTL" << endl;
    ioctl(A2D_STOP, 0, 0);
    DSMSensor::close();
}

int DSMAnalogSensor::readFilterFile(const string& name,unsigned short* coefs,int nexpect)
{
    FILE* fp;
    n_u::Logger::getInstance()->log(LOG_NOTICE,"opening: %s",name.c_str());
    if((fp = fopen(name.c_str(), "r")) == NULL)
        throw n_u::IOException(name,"open",errno);

    int ncoef;
    for(ncoef = 0; ; ) {
	unsigned short val;
	int n = fscanf(fp, "%4hx", &val);
	if (ferror(fp)) {
	    fclose(fp);
	    throw n_u::IOException(name,"fscanf",errno);
	}
	if (feof(fp)) break;
	if (n != 1) {
	    if ((n = getc(fp)) != '#') {
		fclose(fp);
	    	throw n_u::IOException(name,"fscanf",
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
	throw n_u::IOException(name,"fscanf",ost.str());
    }
    return ncoef;
}

int DSMAnalogSensor::gainSetting(float gain)
	throw(n_u::InvalidParameterException)
{
    // todo: screen invalid gains
    return (int)rint(gain * 10.0);
}

void DSMAnalogSensor::init() throw(n_u::InvalidParameterException)
{
    if (initialized) return;
    initialized = true;

}

float DSMAnalogSensor::getTemp() throw(n_u::IOException)
{
    short tval;
    ioctl(A2DTEMP_GET_TEMP, &tval, sizeof(tval));
    return tval * DEGC_PER_CNT;
}

void DSMAnalogSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    A2D_STATUS stat;
    try {
	ioctl(A2D_GET_STATUS,&stat,sizeof(stat));
        float tdeg = getTemp();
	ostr << "<td align=left>";
	ostr << "fifo 1/4ths=" <<
		stat.preFifoLevel[0] << ',' <<
		stat.preFifoLevel[1] << ',' <<
		stat.preFifoLevel[2] << ',' <<
		stat.preFifoLevel[3] << ',' <<
		stat.preFifoLevel[4] << ',' <<
		stat.preFifoLevel[5];
	ostr << ", #badlev=" << stat.nbadFifoLevel + stat.fifoNotEmpty <<
		", #resets=" << stat.resets <<
		", #lost=" << stat.skippedSamples;

        vector<struct chan_info> _channels;
        for (unsigned int ichan = 0; ichan < _channels.size(); ichan++) {
            if (_channels[ichan].gainSetting > 0 && stat.nbad[ichan] > 0) {
		ostr << " ,c=" << ichan << 
			",nbad=" << stat.nbad[ichan] <<
			",stat=" << hex << stat.badval[ichan] << dec;
	    }
	}
	ostr << "temp" << fixed << setprecision(1) <<
	    tdeg << " degC</td>" << endl;

        ostr << "</td>";
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: printStatus: %s",getName().c_str(),
	    ioe.what());
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}

bool DSMAnalogSensor::processTemperature(const Sample* insamp, list<const Sample*>& result) throw()
{
    // number of data values in this raw sample. Should be two, an id and the temperature
    if (insamp->getDataByteLength() / sizeof(short) != 2) return false;

    const signed short* sp = (const signed short*)
    	insamp->getConstVoidDataPtr();
    if (*sp++ != NCAR_A2D_TEMPERATURE_INDEX) return false;

    // cerr << "temperature=" << *sp << ", " << *sp * DEGC_PER_CNT << endl;

    SampleT<float>* osamp = getSample<float>(1);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(_temperatureTag->getId());
    osamp->getDataPtr()[0] = *sp * DEGC_PER_CNT;

    result.push_back(osamp);
    return true;
}

bool DSMAnalogSensor::process(const Sample* insamp,list<const Sample*>& results) throw()
{

// #define DEBUG
#ifdef DEBUG
    static size_t debugcntr = 0;
#endif

    // pointer to raw A2D counts
    const signed short* sp = (const signed short*) insamp->getConstVoidDataPtr();

    // raw data are shorts
    if (insamp->getDataByteLength() % sizeof(short)) {
        badRawSamples++;
        return false;
    }

    // number of short data values in this raw sample.
    unsigned int nvalues = insamp->getDataByteLength() / sizeof(short);

    if (nvalues < 1) {
        badRawSamples++;
        return false;      // nothin
    }

    int nsamp = 1;      // number of A2D samples in this input sample
    int sindex = 0;
    // if more than one sample, the first value is an index
    if (_samples.size() > 1 || _temperatureTag || nvalues == _samples[0].nvars + 1) {
        sindex = *sp++;
        if (sindex < 0 || (sindex >= (signed)_samples.size() && sindex != NCAR_A2D_TEMPERATURE_INDEX)) {
            badRawSamples++;
            return false;
        }
        nvalues--;
    }
    else if (_samples.size() == 1 && (nvalues % _samples[0].nvars) == 0) {
        // One raw sample from A2D contains multiple sweeps
        // of the A2D channels.
        nsamp = nvalues / _samples[0].nvars;
    }
    else {
        badRawSamples++;
        return false;
    }

    // cerr << "sindex=" << sindex << endl;

    if (sindex == NCAR_A2D_TEMPERATURE_INDEX && _temperatureTag)
        return processTemperature(insamp,results);

    const signed short* spend = sp + nvalues;

    for (int isamp = 0; isamp < nsamp; isamp++) {
        struct sample_info* sinfo = &_samples[sindex];
        SampleTag* stag = sinfo->stag;
        const vector<const Variable*>& vars = stag->getVariables();

        SampleT<float>* osamp = getSample<float>(sinfo->nvars);
        dsm_time_t tt = insamp->getTimeTag() + isamp * _deltatUsec;
        osamp->setTimeTag(tt);
        osamp->setId(stag->getId());
        float *fp = osamp->getDataPtr();

        // readCalFile(tt);

        unsigned int ival;
        for (ival = 0; ival < sinfo->nvars && sp < spend;
            ival++,fp++) {
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
#ifdef EVENTUALLY
            else {
                VariableConverter* conv = var->getConverter();
                if (conv) *fp = conv->convert(osamp->getTimeTag(),volts);
                else *fp = volts;
            }
#else
            else *fp = volts;
#endif
        }

        for ( ; ival < sinfo->nvars; ival++) *fp++ = floatNAN;
        results.push_back(osamp);
    }
    return true;
}

void DSMAnalogSensor::readCalFile(dsm_time_t tt) 
    throw(n_u::IOException)
{
    // Read CalFile 
    // gain   bipolar(1=true,0=false) intcp0 slope0 intcp1 slope1
    CalFile* cf = getCalFile();
    if (cf) {
        while(tt >= _calTime) {
            float d[6];
            try {
                int n = cf->readData(d,sizeof d/sizeof(d[0]));
                _calTime = cf->readTime().toUsecs();
            }
            catch(const n_u::EOFException& e)
            {
                _calTime = LONG_LONG_MAX;
            }
            catch(const n_u::IOException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    cf->getCurrentFileName().c_str(),e.what());
                _calTime = LONG_LONG_MAX;
            }
            catch(const n_u::ParseException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    cf->getCurrentFileName().c_str(),e.what());
                _calTime = LONG_LONG_MAX;
            }
        }
    }
}

void DSMAnalogSensor::addSampleTag(SampleTag* tag)
        throw(n_u::InvalidParameterException)
{

    int sindex = _samples.size();       // sample index: 0,1,...
    A2DSensor::addSampleTag(tag);

    const Parameter* tparm = tag->getParameter("temperature");
    if (tparm && tparm->getLength() == 1) {
        _temperatureTag = tag;
        _temperatureRate = irigClockRateToEnum((int)tag->getRate());
        if (_temperatureRate == IRIG_NUM_RATES) {
            ostringstream ost;
            ost << tag->getRate();
            throw n_u::InvalidParameterException(getName(),"temperature sample rate",ost.str());
        }
        return;
    }

    struct sample_info* sinfo = &_samples[sindex];

    const vector<const Variable*>& vars = tag->getVariables();

    int ivar = 0;
    int prevChan = _channels.size() - 1;
    int lastChan = -1;

    vector<const Variable*>::const_iterator vi;
    for (vi = vars.begin(); vi != vars.end(); ++vi,ivar++) {

	const Variable* var = *vi;

        // current code doesn't support variables with length > 1
        if (var->getLength() != 1) 
            throw n_u::InvalidParameterException(getName(),
                var->getName(),"must have length of 1");

	float fgain = 0.0;
        int gainMul = A2DGAIN_MUL;
        int gainDiv = A2DGAIN_DIV;
	bool bipolar = true;

	int ichan = prevChan + 1;
	float corSlope = 1.0;
	float corIntercept = 0.0;
	bool rawCounts = false;

	const std::list<const Parameter*>& params = var->getParameters();
	list<const Parameter*>::const_iterator pi;
	for (pi = params.begin(); pi != params.end(); ++pi) {
	    const Parameter* param = *pi;
	    const string& pname = param->getName();
	    if (pname == "gain") {
		if (param->getLength() != 1)
		    throw n_u::InvalidParameterException(getName(),
		    	pname,"no value");
			
		fgain = param->getNumericValue(0);
	    }
	    else if (pname == "gainMul") {
		if (param->getLength() != 1)
		    throw n_u::InvalidParameterException(getName(),
		    	pname,"no value");
		gainMul = (int)param->getNumericValue(0);
	    }
	    else if (pname == "gainDiv") {
		if (param->getLength() != 1)
		    throw n_u::InvalidParameterException(getName(),
		    	pname,"no value");
		gainDiv = (int)param->getNumericValue(0);
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
	    else if (pname == "rawCounts") {
		if (param->getLength() != 1)
		    throw n_u::InvalidParameterException(getName(),
		    	pname,"no value");
		rawCounts = param->getNumericValue(0);
	    }

	}
        assert(MAX_A2D_CHANNELS >= NUM_NCAR_A2D_CHANNELS);
	if (ichan >= NUM_NCAR_A2D_CHANNELS) {
	    ostringstream ost;
	    ost << NUM_NCAR_A2D_CHANNELS;
	    throw n_u::InvalidParameterException(getName(),"variable",
		string("cannot sample more than ") + ost.str() +
		    string(" channels"));
	}
        if (ichan <= lastChan) {
            ostringstream ost;
            ost << NUM_NCAR_A2D_CHANNELS;
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
        if (fgain == 1.0 && !bipolar) {
            throw n_u::InvalidParameterException(getName(),
                    "gain of 1 and unipolar unsupported","");
        }

	struct chan_info ci;
	memset(&ci,0,sizeof(ci));
        ci.index = -1;
	for (int i = _channels.size(); i <= ichan; i++)
            _channels.push_back(ci);
	ci = _channels[ichan];

        if (ci.gainSetting > 0) {
            ostringstream ost;
            ost << ichan;
            throw n_u::InvalidParameterException(getName(),"variable",
                string("multiple variables for A2D channel ") + ost.str());
        }

	ci.gainSetting = gainSetting(fgain);
	ci.gainMul = gainMul;
	ci.gainDiv = gainDiv;
	ci.bipolar = bipolar;
        ci.rawCounts = rawCounts;
        ci.index = sindex;
	_channels[ichan] = ci;

	/*
	 * A2D chip converts 0:4 V to -32767:32767
	 * 1. Input voltages are first converted by a Gf/256 converter
	 *     where Gf is the gainFactor, Gf=gain*10.
	 * 1.a Then inputs go through a *5.11 multiplier.
	 *    These two steps combined are a  F=gain*0.2
	 *	(actually gain*10*5.11/256 = gain*0.19961)
	 * 2. Then either a 2(bipolar) or 4(unipolar) volt offset is removed.
	 * 3. Then the voltage is inverted.
	 * 4. Converted to counts
	 * 
	 * Example: -10:10 V input, gain=1,bipolar=true
	 *	Gf=gain*10=10,  F=0.2,  offset=2
	 *    Here are the values after the above steps:
	 *	-10:10 -> -2:2 -> -4:0 -> 4:0 -> 32767:-32767
	 *
	 * a2d_driver inverts the counts before passing them
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
	 * voltage value.
	 *
	 *    Vcorr = Vuncorr * corSlope + corIntercept
	 *	    = (cnts * 20 / 65536 / gain + offset) * corSlope +
	 *			corIntercept
	 *	    = cnts * 20 / 65536 / gain * corSlope +
	 *		offset * corSlope + corIntercept
	 */

        if (_channels[ichan].rawCounts) {
            sinfo->convSlopes[ivar] = 1;
            sinfo->convIntercepts[ivar] = 0.;
        } else {
            sinfo->convSlopes[ivar] = 20.0 / 65536 / fgain * corSlope;
            if (bipolar) 
                sinfo->convIntercepts[ivar] = corIntercept;
            else 
                sinfo->convIntercepts[ivar] = corIntercept +
                    10.0 / fgain * corSlope;
        }
    }
    _deltatUsec = (int)rint(USECS_PER_SEC / getScanRate());
}

