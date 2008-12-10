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

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>
#include <iomanip>

using namespace nidas::dynld::raf;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

const float DSMAnalogSensor::TemperatureChamberTemperatures[] = { 12, 22, 32, 42, 52, 62 };	// in Deg C.
const float DSMAnalogSensor::TemperatureTableGain1[][N_DEG] =
{
  {-10.0,-10.0,-10.0,-10.0,-10.0,-10.0 },
  { -9.0, -9.0, -9.0, -9.0, -9.0, -9.0 },
  { -8.0, -8.0, -8.0, -8.0, -8.0, -8.0 },
  { -7.0, -7.0, -7.0, -7.0, -7.0, -7.0 },
  { -6.0, -6.0, -6.0, -6.0, -6.0, -6.0 },
  { -5.0, -5.0, -5.0, -5.0, -5.0, -5.0 },
  { -4.0, -4.0, -4.0, -4.0, -4.0, -4.0 },
  { -3.0, -3.0, -3.0, -3.0, -3.0, -3.0 },
  { -2.0, -2.0, -2.0, -2.0, -2.0, -2.0 },
  { -1.0, -1.0, -1.0, -1.0, -1.0, -1.0 },
  {  0.0,  0.0,  0.0,  0.0,  0.0,  0.0 },
  {  1.0,  1.0,  1.0,  1.0,  1.0,  1.0 },
  {  2.0,  2.0,  2.0,  2.0,  2.0,  2.0 },
  {  3.0,  3.0,  3.0,  3.0,  3.0,  3.0 },
  {  4.0,  4.0,  4.0,  4.0,  4.0,  4.0 },
  {  5.0,  5.0,  5.0,  5.0,  5.0,  5.0 },
  {  6.0,  6.0,  6.0,  6.0,  6.0,  6.0 },
  {  7.0,  7.0,  7.0,  7.0,  7.0,  7.0 },
  {  8.0,  8.0,  8.0,  8.0,  8.0,  8.0 },
  {  9.0,  9.0,  9.0,  9.0,  9.0,  9.0 },
  { 10.0, 10.0, 10.0, 10.0, 10.0, 10.0 },
};

NIDAS_CREATOR_FUNCTION_NS(raf,DSMAnalogSensor)

DSMAnalogSensor::DSMAnalogSensor() :
    A2DSensor(),rtlinux(-1),
    _temperatureTag(0),_temperatureRate(IRIG_NUM_RATES)
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
    setDriverTimeTagUsecs(USECS_PER_MSEC);
    return new DriverSampleScanner();
}

void DSMAnalogSensor::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{

    DSMSensor::open(flags);
    init();

    int nchan;

    ioctl(NIDAS_A2D_GET_NCHAN, &nchan, sizeof(nchan));

    nidas_a2d_config a2dcfg;

    a2dcfg.scanRate = getScanRate();
    a2dcfg.latencyUsecs = (int)(USECS_PER_SEC * getLatency());
    if (a2dcfg.latencyUsecs == 0) a2dcfg.latencyUsecs = USECS_PER_SEC / 10;
    ioctl(NIDAS_A2D_SET_CONFIG, &a2dcfg, sizeof(a2dcfg));

    string filterPath("/usr/local/firmware");
    const Parameter* pparm = getParameter("filterPath");
    
    if (pparm && pparm->getType() == Parameter::STRING_PARAM &&
        pparm->getLength() == 1)
            filterPath = pparm->getStringValue(0);

    ostringstream ost;
    if (getScanRate() >= 1000)
	ost << filterPath << "/fir" << getScanRate()/1000. << "KHz.cfg";
    else
	ost << filterPath << "/fir" << getScanRate() << "Hz.cfg";
    string filtername = ost.str();

    ncar_a2d_ocfilter_config ocfcfg;

    int nexpect = (signed)sizeof(ocfcfg.filter)/sizeof(ocfcfg.filter[0]);
    readFilterFile(filtername,ocfcfg.filter,nexpect);

    ioctl(NCAR_A2D_SET_OCFILTER, &ocfcfg, sizeof(ocfcfg));

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

    if (_temperatureRate != IRIG_NUM_RATES) {
	ioctl(NCAR_A2D_SET_TEMPRATE, &_temperatureRate, sizeof(_temperatureRate));
    }

    ioctl(NCAR_A2D_RUN, 0, 0);
}

void DSMAnalogSensor::close() throw(n_u::IOException)
{
    // cerr << "doing A2D_STOP ioctl" << endl;
    ioctl(NCAR_A2D_STOP, 0, 0);
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

void DSMAnalogSensor::setA2DParameters(int ichan,int gain,int bipolar)
            throw(n_u::InvalidParameterException)
{
    switch (gain) {
    case 1:
    case 2:
    case 4:
        break;
    default:
        {
            ostringstream ost;
            ost << "channel=" << ichan << ", gain=" << gain << " is not supported";
            throw n_u::InvalidParameterException(getName(),
                "gain",ost.str());
        }
    }
    if (gain == 1 && !bipolar) {
            ostringstream ost;
            ost << "channel=" << ichan << ", gain=" << gain << ",bipolar=F is not supported";
            throw n_u::InvalidParameterException(getName(),
                "gain & offset",ost.str());
    }
    A2DSensor::setA2DParameters(ichan,gain,bipolar);

}

void DSMAnalogSensor::getBasicConversion(int ichan,
    float& intercept, float& slope) const
{
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
     * back to user space, so for purposes here, it is as if
     * the A2D converts -4:0 volts to -32767:32767 counts
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
    slope = 20.0 / 65536 / getGain(ichan);
    if (getBipolar(ichan)) intercept = 0.0;
    else intercept = 10.0 / getGain(ichan);
}

float DSMAnalogSensor::getTemp() throw(n_u::IOException)
{
    short tval;
    ioctl(NCAR_A2D_GET_TEMP, &tval, sizeof(tval));
    return tval * DEGC_PER_CNT;
}

void DSMAnalogSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
	return;
    }

    ncar_a2d_status stat;
    try {
	ioctl(NCAR_A2D_GET_STATUS,&stat,sizeof(stat));
        float tdeg = getTemp();
	ostr << "<td align=left>";
	ostr << "fifo 1/4ths=" <<
		stat.preFifoLevel[0] << ',' <<
		stat.preFifoLevel[1] << ',' <<
		stat.preFifoLevel[2] << ',' <<
		stat.preFifoLevel[3] << ',' <<
		stat.preFifoLevel[4] << ',' <<
		stat.preFifoLevel[5];
	ostr << ", #resets=" << stat.resets <<
		", #lost=" << stat.skippedSamples;

	ostr << ", temp=" << fixed << setprecision(1) <<
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

    float value = *sp * DEGC_PER_CNT;
    if (value > 256) value -= 512;
    _currentTemperature = osamp->getDataPtr()[0] = value;

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
        _badRawSamples++;
        return false;
    }

    // number of short data values in this raw sample.
    int nvalues = insamp->getDataByteLength() / sizeof(short);

    if (nvalues < 1) {
        _badRawSamples++;
        return false;      // nothin
    }

    int nsamp = 1;      // number of A2D samples in this input sample
    int sindex = 0;
    // if more than one sample, the first value is an index
    if (_sampleInfos.size() > 1 || _temperatureTag || nvalues == _sampleInfos[0]->nvars + 1) {
        sindex = *sp++;
        if (sindex < 0 || (sindex >= (signed)_sampleInfos.size() && sindex != NCAR_A2D_TEMPERATURE_INDEX)) {
            _badRawSamples++;
            return false;
        }
        nvalues--;
    }
    else if (_sampleInfos.size() == 1 && (nvalues % _sampleInfos[0]->nvars) == 0) {
        // One raw sample from A2D contains multiple sweeps
        // of the A2D channels.
        nsamp = nvalues / _sampleInfos[0]->nvars;
    }
    else {
        _badRawSamples++;
        return false;
    }

    // cerr << "sindex=" << sindex << endl;

    if (sindex == NCAR_A2D_TEMPERATURE_INDEX && _temperatureTag)
        return processTemperature(insamp,results);

    const signed short* spend = sp + nvalues;

    readCalFile(insamp->getTimeTag());

    for (int isamp = 0; isamp < nsamp; isamp++) {
        A2DSampleInfo* sinfo = _sampleInfos[sindex];
        const SampleTag* stag = sinfo->stag;
        const vector<const Variable*>& vars = stag->getVariables();

        SampleT<float>* osamp = getSample<float>(sinfo->nvars);
        dsm_time_t tt = insamp->getTimeTag() + isamp * _deltatUsec;
        osamp->setTimeTag(tt);
        osamp->setId(stag->getId());
        float *fp = osamp->getDataPtr();

        int ival;
        for (ival = 0; ival < sinfo->nvars && sp < spend;
            ival++,fp++) {
            short sval = *sp++;
            int ichan = sinfo->channels[ival];
            if (sval == -32768 || sval == 32767) {
                *fp = floatNAN;
                continue;
            }

            float volts = getIntercept(ichan) + getSlope(ichan) * sval;

            const Variable* var = vars[ival];
            if (volts < var->getMinValue() || volts > var->getMaxValue())
                *fp = floatNAN;
	    else if (getApplyVariableConversions()) {
                VariableConverter* conv = var->getConverter();
                if (conv) *fp = conv->convert(osamp->getTimeTag(),volts);
                else *fp = volts;
            }
            else *fp = volts;
        }
        for ( ; ival < sinfo->nvars; ival++) *fp++ = floatNAN;
        results.push_back(osamp);
    }
    return true;
}

void DSMAnalogSensor::readCalFile(dsm_time_t tt) 
    throw(n_u::IOException)
{
    // Read CalFile  containing the following fields after the time
    // gain bipolar(1=true,0=false) intcp0 slope0 intcp1 slope1 ... intcp7 slope7
    CalFile* cf = getCalFile();
    if (!cf) return;
    while(tt >= _calTime) {
        int nd = 2 + getMaxNumChannels() * 2;
        float d[nd];
        try {
            int n = cf->readData(d,nd);
            _calTime = cf->readTime().toUsecs();
            if (n < 2) continue;
            int cgain = (int)d[0];
            int cbipolar = (int)d[1];
            for (int i = 0;
                i < std::min((n-2)/2,getMaxNumChannels()); i++) {
                    int gain = getGain(i);
                    int bipolar = getBipolar(i);
                    if ((cgain < 0 || gain == cgain) &&
                        (cbipolar < 0 || bipolar == cbipolar))
                        setConversionCorrection(i,d[2+i*2],d[3+i*2]);
            }
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


float DSMAnalogSensor::voltageActual(float voltageMeasured)
{
    // Locate column
    int col = 0;

    if (_currentTemperature < TemperatureChamberTemperatures[0])
    {
        return voltageMeasured; // Can't extrapolate.  Probably should print warning.
    }

    for (col = 0; _currentTemperature >= TemperatureChamberTemperatures[col]; ++col)
        ;

    --col;

    // Locate row
    int row = 10 + (int)voltageMeasured;
    if (voltageMeasured < 0.0)
      --row;

    float tempFract =
        (_currentTemperature - TemperatureChamberTemperatures[col]) /
        (TemperatureChamberTemperatures[col+1] - TemperatureChamberTemperatures[col]);

    float voltFract = voltageMeasured - (int)voltageMeasured;

    if (voltageMeasured < 0.0)
        voltFract += 1.0;

    // Interpolation in two dimensions.
    float voltageActual =
        (1.0 - voltFract) * (1.0 - tempFract) * TemperatureTableGain1[row][col] +
        voltFract * (1.0 - tempFract) * TemperatureTableGain1[row+1][col] +
        voltFract * tempFract * TemperatureTableGain1[row+1][col+1] +
        (1.0 - voltFract) * tempFract * TemperatureTableGain1[row][col+1];

    return voltageActual;
}


void DSMAnalogSensor::addSampleTag(SampleTag* tag)
        throw(n_u::InvalidParameterException)
{
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
    _deltatUsec = (int)rint(USECS_PER_SEC / getScanRate());
}

