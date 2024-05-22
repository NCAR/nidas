// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "A2DSensor.h"
#include <nidas/core/Variable.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/CalFile.h>
#include <nidas/linux/diamond/dmd_mmat.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

A2DSensor::A2DSensor(int nchan) :
    DSMSensor(), _maxNumChannels(nchan),
    _calFile(0), _outputMode(Engineering),
    _sampleCfgs(),_sampleInfos(),
    _badRawSamples(0),
    _scanRate(0), _prevChan(-1)
{
    setLatency(0.1);
}

A2DSensor::~A2DSensor()
{
    for (unsigned int i = 0; i < _sampleCfgs.size(); i++)
        delete _sampleCfgs[i];
}

void A2DSensor::open(int flags)
{
    DSMSensor::open(flags);
}

void A2DSensor::close()
{
    DSMSensor::close();
}

void A2DSensor::init()
{
    DSMSensor::init();
    const map<string,CalFile*>& cfs = getCalFiles();
    // Just use the first file.
    if (!cfs.empty()) _calFile = cfs.begin()->second;
}

void A2DSensor::setGainBipolar(int ichan, int gain, int bipolar)
{
    if (ichan < 0 || ichan >=  getMaxNumChannels()) return;
    getFinalConverter()->setGain(ichan, gain);
    getFinalConverter()->setBipolar(ichan, bipolar);
}

bool A2DSensor::process(const Sample* insamp,list<const Sample*>& results) throw()
{
    // pointer to raw A2D counts
    const short* sp = (const short*) insamp->getConstVoidDataPtr();

    // raw data are shorts
    if (insamp->getDataByteLength() % sizeof(short)) {
        _badRawSamples++;
        return false;
    }

    // number of short values in this raw sample.
    int nvalues = insamp->getDataByteLength() / sizeof(short);
    if (nvalues < 1) {
        _badRawSamples++;
        return false;      // nothin
    }
    const short* spend = sp + nvalues;

    unsigned int sindex = 0;
    // if more than one sample, the first value is an index
    if (_sampleInfos.size() > 1 || nvalues == _sampleInfos[0].nvalues + 1) {
        sindex = *sp++;
        if (sindex >=  _sampleInfos.size()) {
            _badRawSamples++;
            return false;
        }
    }

    if (getOutputMode() != Counts && _calFile) {
        try {
            getFinalConverter()->readCalFile(_calFile, insamp->getTimeTag());
        }
        catch(const n_u::EOFException& e) {
        }
        catch(const n_u::IOException& e) {
            n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                _calFile->getCurrentFileName().c_str(),e.what());
            getFinalConverter()->setNAN();
            _calFile = 0;
        }       
        catch(const n_u::ParseException& e) {
            n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                _calFile->getCurrentFileName().c_str(),e.what());
            getFinalConverter()->setNAN();
            _calFile = 0; 
        }               
    }           

    A2DSampleInfo& sinfo = _sampleInfos[sindex];
    SampleTag* stag = sinfo.stag;
    const vector<Variable*>& vars = stag->getVariables();

    SampleT<float>* osamp = getSample<float>(sinfo.nvalues);
    osamp->setTimeTag(insamp->getTimeTag());
    osamp->setId(stag->getId());
    float *fp = osamp->getDataPtr();
    const float* fpend = fp + sinfo.nvalues;

    for (unsigned int ivar = 0; ivar < vars.size(); ivar++) {
        Variable* var = vars[ivar];
        int ichan = sinfo.channels[ivar];

        for (unsigned int ival = 0; sp < spend && ival < var->getLength();
             ival++, fp++)
        {
            short sval = *sp++;
            if (getOutputMode() == Counts) {
                *fp = sval;
                continue;
            }

            if (sval == -32768 || sval == 32767) {
                *fp = floatNAN;
                continue;
            }
            
            float fval = getInitialConverter()->convert(ichan, sval);
            fval = getFinalConverter()->convert(ichan, fval);
            *fp = fval;
        }
    }

    for ( ; fp < fpend; ) *fp++ = floatNAN;
    if (getOutputMode() == Engineering) applyConversions(stag, osamp);
    results.push_back(osamp);

    return true;
}

void A2DSensor::validate()
{
    /*
     Hypothetical, somewhat complex sensor configuration:
     User wants to sample channels 0,4 and 5 at 10/s,
     and channels 1 and 2 at 100/s:

        <sensor>
            <parameter name="rate" value="1000"/>   // sample rate 1KHz
            <sample id="1" rate="10">               // output rate 10Hz
                <parameter name="filter" value="boxcar" type="string"/>
                <parameter name="numpoints" value="4" type="int"/>
                <variable name="IN0"/>  // default channel 0
                <variable name="IN4">
                   <parameter name="channel" value="4" type="int"/>
                </variable>
                <variable name="IN5"/>  // channel=5 is 1 plus previous
                </variable>
            </sample>
            <sample id="2" rate="100">              // output rate 100Hz
                <parameter name="filter" value="boxcar" type="string"/>
                <parameter name="numpoints" value="4" type="int"/>
                <variable name="IN1">
                   <parameter name="channel" value="1" type="int"/>
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
    DSMSensor::validate();

    // ncar: linear, then tweaked by cal file
    //      initial:
    //          gain==4, default factors
    //          else default factors
    //      final:
    //          gain==4: 0,1 then set from cal file
    //              adjust for temp compensation  between initial and final
    //          else: 0,1 update from cal file
    //          XML cor values: set final
    //
    // DSC: linear from default conversion, then poly from cal file
    //      initial: default conversion
    //      final: poly from cal file
    //          XML cor values: set final. Get overwritten from cal file
    //
    // serial: linear from default conversion, then poly from cal file
    //      initial: for now, 0,1, otherwise default conversion
    //      final: poly from cal file
    //          XML cor values: set final. Get overwritten from cal file

    assert(getInitialConverter());
    assert(getFinalConverter());

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
        else if (pname == "latency") {
                if (param->getLength() != 1)
			throw n_u::InvalidParameterException(getName(),"parameter",
                        "bad latency  parameter");
                setLatency((int)param->getNumericValue(0));
        }
	else if (pname == "outputmode") {
	    if (param->getType() != Parameter::STRING_PARAM ||
		param->getLength() != 1)
		throw n_u::InvalidParameterException(getName(),"outputmode",
		    "parameter is not a string");
	    string fname = param->getStringValue(0);
            for (unsigned int i = 0; i < fname.length(); ++i)
                fname[i] = tolower(fname[i]);
	    if (fname == "counts") _outputMode = Counts;
	    else if (fname == "volts") _outputMode = Volts;
	    else if (fname == "engineering") _outputMode = Engineering;
	    else throw n_u::InvalidParameterException(getName(),"outputmode",       
		    fname + " is not supported");
            }
    }

    const std::list<SampleTag*>& tags = getSampleTags();
    std::list<SampleTag*>::const_iterator ti = tags.begin();

    for ( ; ti != tags.end(); ++ti) {
        SampleTag* tag = *ti;

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
        bool temperature = false;

        // Default time average rate is the sample rate.
        // User can choose a time average rate that is a multiple
        // of the sample rate, in which case the results of the 
        // time averaging are subsampled by a pickoff of 1 out of
        // every N time averages, where N = timeavgRate / sample rate.
        int timeavgRate = tag->getRate();

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
                    else if (fname == "timeavg") filterType = NIDAS_FILTER_TIMEAVG;
                    else throw n_u::InvalidParameterException(getName(),"sample",
                            fname + " filter is not supported");
            }
            else if (pname == "numpoints") {
                    if (param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),"sample",
                            "bad numpoints parameter");
                    boxcarNpts = (int)param->getNumericValue(0);
            }
            else if (pname == "rate") {
                    if (param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),"sample",
                            "bad rate parameter");
                    timeavgRate = (int)param->getNumericValue(0);
            }
            else if (pname == "temperature") {
                    if (param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),"sample",
                            "bad temperature parameter");
                    temperature = (int)param->getNumericValue(0);
            }
        }
        if (temperature) continue;

        if (filterType == NIDAS_FILTER_BOXCAR && boxcarNpts <= 0) {
            throw n_u::InvalidParameterException(getName(),"numpoints",
                "numpoints parameter must be > 0 with boxcar filter");

        }
        if (filterType == NIDAS_FILTER_TIMEAVG) {
            ostringstream ost;
            if (timeavgRate <= 0) {
                ost << timeavgRate << " Hz must be > 0";
                throw n_u::InvalidParameterException(getName(), 
                    "timeavg rate", ost.str());
            }
            if (fmod((double) timeavgRate, tag->getRate()) != 0.0) {
                ost << timeavgRate <<
                    " Hz must be a multiple of the sample rate=" <<
                    tag->getRate() << " Hz";
                throw n_u::InvalidParameterException(getName(),
                    "timeavg rate", ost.str());
            }
        }

        int sindex = _sampleInfos.size();       // sample index, 0,1,...

        const vector<Variable*>& vars = tag->getVariables();
        int nvars = vars.size();

        A2DSampleInfo sinfo(nvars);
        sinfo.stag = tag;

        A2DSampleConfig* scfg;

        switch (filterType) {
        case NIDAS_FILTER_TIMEAVG:
            scfg = new A2DTimeAvgConfig(timeavgRate);
            break;
        case NIDAS_FILTER_BOXCAR:
            scfg = new A2DBoxcarConfig(boxcarNpts);
            break;
        default:
            scfg = new A2DSampleConfig();
            break;
        }
        nidas_a2d_sample_config& ncfg = scfg->cfg();

        ncfg.sindex = sindex;
        ncfg.nvars = nvars;
        ncfg.rate = rate;
        ncfg.filterType = filterType;

        int nvalues = 0;
        for (int iv = 0; iv < nvars; iv++) {
            Variable* var = vars[iv];

            float fgain = 0.0;
            int bipolar = -1;   // unknown

            int ichan = _prevChan + 1;
            float corSlope = 1.0;
            float corIntercept = 0.0;
            // unused: bool rawCounts = false;

            nvalues += var->getLength();

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
                else if (pname == "rawCounts") {
                    if (param->getLength() != 1)
                        throw n_u::InvalidParameterException(getName(),
                            pname,"no value");
                    // even though rawCounts is not used, this code still
                    // validates the xml, ie, the parameter must have a value
                    // same as before.  getNumericValue() does not check that
                    // the underlying Parameter type is numeric, it just
                    // returns nan if not, so there's no need to call it in
                    // case it triggers an exception.
                    //
                    // unused: rawCounts = param->getNumericValue(0);
                }
            }
            if (ichan < 0 || ichan >= getMaxNumChannels()) {
                ostringstream ost;
                ost << "value=" << ichan << " is outside the range 0:" <<
                    (getMaxNumChannels() - 1);
                throw n_u::InvalidParameterException(getName(),
                        "channel",ost.str());
            }

            if (fgain == 0.0) fgain = getGain(ichan);
            if (fmodf(fgain,1.0) != 0.0) {
                ostringstream ost;
                ost << "channel " << ichan << " gain=" << fgain << " but must be an integer";
                throw n_u::InvalidParameterException(getName(),
                        "gain",ost.str());
            }
            int gain = (int)fgain;
            if (gain == 0) {
                ostringstream ost;
                ost << "channel " << ichan << " gain not specified";
                throw n_u::InvalidParameterException(getName(),
                        "gain",ost.str());
            }
            if (bipolar < 0) bipolar = getBipolar(ichan);
            if (bipolar < 0) {
                ostringstream ost;
                ost << "channel " << ichan << " polarity not specified";
                throw n_u::InvalidParameterException(getName(),
                        "bipolar",ost.str());
            }

            // derived classes can throw InvalidParameterException
            // if they don't support a certain gain or polarity.
            setGainBipolar(ichan, gain, bipolar);

            // tweak final Converter with corIntercept, corSlope
            float cfact[2];
            cfact[0] = corIntercept;
            cfact[1] = corSlope;
            getFinalConverter()->set(ichan, cfact, sizeof(cfact) / sizeof(cfact[0]));

            var->setA2dChannel(ichan);
            sinfo.channels[iv] = ichan;

            ncfg.channels[iv] = ichan;
            ncfg.gain[iv] = gain;
            ncfg.bipolar[iv] = bipolar;
            _prevChan = ichan;
        }
        sinfo.nvalues = nvalues;

        _sampleInfos.push_back(sinfo);
        _sampleCfgs.push_back(scfg);
    }
}

