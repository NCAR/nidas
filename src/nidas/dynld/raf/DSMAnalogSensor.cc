// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/CalFile.h>
#include <nidas/core/DSMEngine.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>
#include <iomanip>

using namespace nidas::dynld::raf;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

const float DSMAnalogSensor::TemperatureChamberVoltagesGain4[] = { 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5 };
const float DSMAnalogSensor::TemperatureTableGain4[][N_COEFF] =
{
  { -1.768949e-03, -1.566086e-04, 5.010940e-06 },
  { -7.218792e-03, -8.230977e-06, 4.673119e-06 },
  { -1.351594e-02, 1.499760e-04, 4.499533e-06 },
  { -1.900132e-02, 2.957921e-04, 4.223245e-06 },
  { -2.520223e-02, 4.508312e-04, 4.068070e-06 },
  { -3.063259e-02, 5.960058e-04, 3.779620e-06 },
  { -3.680750e-02, 7.507708e-04, 3.620800e-06 },
  { -4.228688e-02, 8.962391e-04, 3.362695e-06 },
  { -4.873845e-02, 1.054765e-03, 3.243052e-06 },
};

NIDAS_CREATOR_FUNCTION_NS(raf,DSMAnalogSensor)

DSMAnalogSensor::DSMAnalogSensor() :
    A2DSensor(),_deltatUsec(0),
    _temperatureTag(0),_temperatureRate(IRIG_NUM_RATES),
    _calFile(0),_calTime(0),_outputMode(Volts),_currentTemperature(40.0)
{
    setScanRate(500);   // lowest scan rate supported by card
    setLatency(0.1);
}

DSMAnalogSensor::~DSMAnalogSensor()
{
}

IODevice* DSMAnalogSensor::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner* DSMAnalogSensor::buildSampleScanner()
    throw(n_u::InvalidParameterException)
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
    string ioctlcmd;

    try {

        ioctlcmd = "NIDAS_A2D_GET_NCHAN";
        ioctl(NIDAS_A2D_GET_NCHAN, &nchan, sizeof(nchan));

        nidas_a2d_config a2dcfg;

        a2dcfg.scanRate = getScanRate();
        a2dcfg.latencyUsecs = (int)(USECS_PER_SEC * getLatency());
        if (a2dcfg.latencyUsecs == 0) a2dcfg.latencyUsecs = USECS_PER_SEC / 10;

        ioctlcmd = "NIDAS_A2D_SET_CONFIG";
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

        ioctlcmd = "NIDAS_A2D_SET_OCFILTER";
        ioctl(NCAR_A2D_SET_OCFILTER, &ocfcfg, sizeof(ocfcfg));

        for(unsigned int i = 0; i < _sampleCfgs.size(); i++) {
            struct nidas_a2d_sample_config& scfg = _sampleCfgs[i]->cfg();

            for (int j = 0; j < scfg.nvars; j++) {
                if (scfg.channels[j] >= nchan) {
                    ostringstream ost;
                    ost << "channel number " << scfg.channels[j] <<
                        " is out of range, max=" << nchan;
                    throw n_u::InvalidParameterException(getName(),
                        "channel",ost.str());
                }
            }

            ioctlcmd = "NIDAS_A2D_CONFIG_SAMPLE";
            ioctl(NIDAS_A2D_CONFIG_SAMPLE, &scfg,
                sizeof(struct nidas_a2d_sample_config)+scfg.nFilterData);
        }

        if (_temperatureRate != IRIG_NUM_RATES) {
            ioctlcmd = "NIDAS_A2D_SET_TEMPRATE";
            ioctl(NCAR_A2D_SET_TEMPRATE, &_temperatureRate, sizeof(_temperatureRate));
        }

        ioctlcmd = "NIDAS_A2D_RUN";
        ioctl(NCAR_A2D_RUN, 0, 0);
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: open ioctl %s failed: %s",
	    getName().c_str(),ioctlcmd.c_str(),ioe.what());
        throw ioe;
    }

    DSMEngine::getInstance()->registerSensorWithXmlRpc(getDeviceName(),this);
}

void DSMAnalogSensor::close() throw(n_u::IOException)
{
    DSMSensor::close();
}

void DSMAnalogSensor::init() throw(nidas::util::InvalidParameterException)
{
    const map<string,CalFile*>& cfs = getCalFiles();
    // Just use the first file. If, for some reason a second calibration
    // is applied to this sensor, we must differentiate them by name.
    // Note this calibration is separate from that applied to each variable.
    if (!cfs.empty()) _calFile = cfs.begin()->second;
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
    if (getOutputMode() == Counts) {
        slope = 1.0;
        intercept = 0.0;
    }
    else {
        slope = 20.0 / 65536 / getGain(ichan);
        if (getBipolar(ichan)) intercept = 0.0;
        else intercept = 10.0 / getGain(ichan);
    }
}

void DSMAnalogSensor::setConversionCorrection(int ichan, float corIntercept,
    float corSlope) throw(n_u::InvalidParameterException)
{
    if (getOutputMode() == Counts) {
        corSlope = 1.0;
        corIntercept = 0.0;
    }
    A2DSensor::setConversionCorrection(ichan, corIntercept, corSlope);

    /* For the A/D temperature compensation that we are doing for gain of 4
     * channels, do not multiply through BasicConversion here.  Just return
     * the slope/offset from the cal file.
     */
    if (getGain(ichan) == 4) {
        _convSlopes[ichan] = corSlope;
        _convIntercepts[ichan] = corIntercept;

        // Stash for ongoing use in the process method.
        getBasicConversion(ichan, _basIntercept[ichan], _basSlope[ichan]);
    }
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
    if (_sampleInfos.size() > 1 || _temperatureTag || nvalues == _sampleInfos[0].nvars + 1) {
        sindex = *sp++;
        if (sindex < 0 || (sindex >= (signed)_sampleInfos.size() && sindex != NCAR_A2D_TEMPERATURE_INDEX)) {
            _badRawSamples++;
            return false;
        }
        nvalues--;
    }
    else if (_sampleInfos.size() == 1 && (nvalues % _sampleInfos[0].nvars) == 0) {
        // One raw sample from A2D contains multiple sweeps
        // of the A2D channels.
        nsamp = nvalues / _sampleInfos[0].nvars;
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
        A2DSampleInfo& sinfo = _sampleInfos[sindex];
        SampleTag* stag = sinfo.stag;
        const vector<Variable*>& vars = stag->getVariables();

        SampleT<float>* osamp = getSample<float>(sinfo.nvars);
        dsm_time_t tt = insamp->getTimeTag() + isamp * _deltatUsec;
        osamp->setTimeTag(tt - getLagUsecs());
        osamp->setId(stag->getId());
        float *fp = osamp->getDataPtr();

        int ival;
        for (ival = 0; ival < sinfo.nvars && sp < spend; ival++,fp++) {
            short sval = *sp++;
            int ichan = sinfo.channels[ival];
            if (sval == -32768 || sval == 32767) {
                *fp = floatNAN;
                continue;
            }

            float val;

            /* Apply analog card temperature compensation.  At this time it is only
             * applied for 0-5 volt analog variables.
             */
            if (getGain(ichan) == 4) {
                float basIntercept, basSlope;
                getBasicConversion(ichan, basIntercept, basSlope);
                val = _basIntercept[ichan] + _basSlope[ichan] * sval;

                // Apply temperature compensation.
                val = getIntercept(ichan) + getSlope(ichan) * voltageActual(val);
            }
            else {
                // Default, do as before.
                val = getIntercept(ichan) + getSlope(ichan) * sval;
            }

            Variable* var = vars[ival];
	    if (getApplyVariableConversions()) {
                VariableConverter* conv = var->getConverter();
                if (conv) val = conv->convert(osamp->getTimeTag(),val);
            }
            /* Screen values outside of min,max after the conversion */
            if (val < var->getMinValue() || val > var->getMaxValue())
                val = floatNAN;
            *fp = val;
        }
        for ( ; ival < sinfo.nvars; ival++) *fp++ = floatNAN;
        results.push_back(osamp);
    }
    return true;
}

void DSMAnalogSensor::readCalFile(dsm_time_t tt) 
    throw(n_u::IOException)
{
    if (!_calFile) return;

    if (getOutputMode() == Counts)
        return;

    // Read CalFile  containing the following fields after the time
    // gain bipolar(1=true,0=false) intcp0 slope0 intcp1 slope1 ... intcp7 slope7

    while(tt >= _calTime) {
        int nd = 2 + getMaxNumChannels() * 2;
        float d[nd];
        try {
            int n = _calFile->readData(d,nd);
            _calTime = _calFile->readTime().toUsecs();
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
                _calFile->getCurrentFileName().c_str(),e.what());
            _calTime = LONG_LONG_MAX;
        }
        catch(const n_u::ParseException& e)
        {
            n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                _calFile->getCurrentFileName().c_str(),e.what());
            _calTime = LONG_LONG_MAX;
        }
    }
}


float DSMAnalogSensor::voltageActual(float voltageMeasured)
{
  // Don't extrapolate, just return end-cal.
  if (voltageMeasured <= TemperatureChamberVoltagesGain4[0]) {
      return voltageMeasured - SecondPoly(_currentTemperature, TemperatureTableGain4[0]);
  }

  if (voltageMeasured >= TemperatureChamberVoltagesGain4[N_G4_VDC-1]) {
      return voltageMeasured - SecondPoly(_currentTemperature, TemperatureTableGain4[N_G4_VDC-1]);
  }

  for (int i = 1; i < N_G4_VDC; ++i) {
    if (voltageMeasured == TemperatureChamberVoltagesGain4[i])
        return voltageMeasured - SecondPoly(_currentTemperature, TemperatureTableGain4[i]);

    if (voltageMeasured < TemperatureChamberVoltagesGain4[i]) {
        float v0 = SecondPoly(_currentTemperature, TemperatureTableGain4[i-1]);
        float v1 = SecondPoly(_currentTemperature, TemperatureTableGain4[i]);
        float diff = TemperatureChamberVoltagesGain4[i] - TemperatureChamberVoltagesGain4[i-1];
        float diffv = voltageMeasured - TemperatureChamberVoltagesGain4[i-1];

        return voltageMeasured - (v0 + ((v1 - v0) / diff) * diffv);
    }
  }

  cerr << "voltageActual: returning input voltage.\n";
  return voltageMeasured;	// Shouldn't get here, but a catchall.
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

void DSMAnalogSensor::executeXmlRpc(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw()
{
    string action = "null";
    if (params.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
        action = string(params["action"]);
    }
    else if (params.getType() == XmlRpc::XmlRpcValue::TypeArray) {
        action = string(params[0]["action"]);
    }

    if      (action == "testVoltage") testVoltage(params,result);
    else if (action == "getA2DSetup") getA2DSetup(params,result);
    else {
        string errmsg = "XmlRpc error: " + getName() + ": no such action " + action;
        PLOG(("Error: ") << errmsg);
        result = errmsg;
        return;
    }
}

void DSMAnalogSensor::getA2DSetup(XmlRpc::XmlRpcValue&, XmlRpc::XmlRpcValue& result)
        throw()
{
    // extract the current channel setup
    ncar_a2d_setup setup;
    try {
        ioctl(NCAR_A2D_GET_SETUP, &setup, sizeof(setup));
    }
    catch(const n_u::IOException& e) {
        string errmsg = "XmlRpc error: getA2DSetup: " + getName() + ": " + e.what();
        PLOG(("") << errmsg);
        result = errmsg;
        return;
    }

    for (int i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
        result["gain"][i]   = setup.gain[i];
        result["offset"][i] = setup.offset[i];
        result["calset"][i] = setup.calset[i];
    }
    result["vcal"]      = setup.vcal;
    DLOG(("%s: result:",getName().c_str()) << result.toXml());
}

void DSMAnalogSensor::testVoltage(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw()
{
    struct ncar_a2d_cal_config calConf;
    int voltage = 0;
    int calset  = 0;
    int state   = 0;

    string errmsg = "XmlRpc error: testVoltage: " + getName();

    if (params.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
        voltage = params["voltage"];
        calset  = params["calset"];
        state   = params["state"];
    }
    else if (params.getType() == XmlRpc::XmlRpcValue::TypeArray) {
        voltage = params[0]["voltage"];
        calset  = params[0]["calset"];
        state   = params[0]["state"];
    }
    if (calset < 0 || 0xff < calset) {
        char hexstr[50];
        sprintf(hexstr, "0x%x", calset);
        errmsg += ": invalid calset: " + string(hexstr);
        PLOG(("") << errmsg);
        result = errmsg;
        return;
    }
    calConf.vcal = voltage;
    calConf.state = state;

    for (int i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
        calConf.calset[i] = (calset & (1 << i)) ? 1 : 0;

    // set the test voltage and channel(s)
    try {
        ioctl(NCAR_A2D_SET_CAL, &calConf, sizeof(ncar_a2d_cal_config));
    }
    catch(const n_u::IOException& e) {
        string errmsg = "XmlRpc error: testVoltage: " + getName() + ": " + e.what();
        PLOG(("") << errmsg);
        result = errmsg;
        return;
    }
    result = "success";
    DLOG(("%s: result:",getName().c_str()) << result.toXml());
}
