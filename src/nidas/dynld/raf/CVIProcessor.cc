/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/CVIProcessor.cc $
 ********************************************************************

*/

#include <nidas/dynld/raf/CVIProcessor.h>
#include <nidas/dynld/raf/CVI_LV_Input.h>


#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,CVIProcessor)

CVIProcessor::CVIProcessor():
    _sorter("CVISorter"),
    _avgWrapper(&_averager),
    _resampler(0),_rate(1.0),
    _lvSensor(0)
{
    setName("CVIProcessor");
}

/*
 * Copy constructor
 */
CVIProcessor::CVIProcessor(const CVIProcessor& x):
	SampleIOProcessor(x),
        _sorter("CVISorter"),
        _avgWrapper(&_averager),
        _resampler(0),_rate(x._rate),
        _lvSensor(0)
{
}

CVIProcessor::~CVIProcessor()
{
}

CVIProcessor* CVIProcessor::clone() const
{
    return new CVIProcessor(*this);
}

void CVIProcessor::addSampleTag(SampleTag* tag)
	throw(n_u::InvalidParameterException)
{
    VariableIterator vi = tag->getVariableIterator();
    for ( ; vi.hasNext(); ) {
        const Variable* var = vi.next();
        _variables.push_back(var);
        _varMatched.push_back(false);
    }
    _rate = tag->getRate();
    if (_rate > 0.0) {
        _averager.setAveragePeriod((int)rint(MSECS_PER_SEC/_rate));
        _sorter.setLengthMsecs((int)rint(MSECS_PER_SEC/_rate/10.));
    }
}

void CVIProcessor::connect(SampleInput* input) throw(n_u::IOException)
{
    /*
     * Find a match with a variable from the CVI sample:
     * add the variable to the SampleAverager
     * keep track of which one it is in the averager
     * 
     * Connect the CVIOutput to the averager
     * 
     * CVIOutput:
     *      has the sample tag
     *      loop over averager sample, putting values
     *      where they belong. Add douts, vouts
     *      Send sample.
     * What about if deviations are wanted?  Call them  XXXX_dev, or XXXX_sd
     * in the CVI sample, and compute a deviation.
     */

    // if it is a raw sample from a sensor, then
    // sensor will be non-NULL.
    DSMSensor* sensor = 0;

    SampleTagIterator inti = input->getSampleTagIterator();
    for ( ; inti.hasNext(); ) {
        const SampleTag* intag = inti.next();
        dsm_sample_id_t sensorId = intag->getId() - intag->getSampleId();
        sensor = Project::getInstance()->findSensor(sensorId);

#ifdef DEBUG
        if (sensor) cerr << "CVIProcessor::connect sensor=" <<
            sensor->getName() << " (" << GET_DSM_ID(sensorId) << ',' << 
            GET_SHORT_ID(sensorId) << ')' << endl;
        else cerr << "CVIProcessor no sensor" << endl;
#endif

        if (sensor && dynamic_cast<CVI_LV_Input*>(sensor)) {
            _lvSensor = sensor;
            attachLVInput(intag);
            return;
        }
        // sensor->setApplyVariableConversions(true);

        bool varMatch = false;

        for (VariableIterator invi = intag->getVariableIterator();
            invi.hasNext(); ) {
            const Variable* invar = invi.next();

            for (unsigned int i = 0; i < _variables.size(); i++) {
                const Variable* myvar = _variables[i];
		// variable match
		if (*invar == *myvar) {
                    _averager.addVariable(invar);
                    _varMatched[i] = true;
                    varMatch = true;
                    // cerr << "CVIProcessor match var=" << myvar->getName() << endl;
                }
            }
        }

        if (varMatch && sensor) {
            sensor->addSampleClient(&_sorter);
            _sorter.addSampleTag(intag,sensor);
            _sorter.addSampleClient(&_averager);
            if (!_sorter.isRunning()) _sorter.start();

            sensor->addRawSampleClient(&_sorter);
            _sorter.addSampleTag(sensor->getRawSampleTag(),sensor);
            sensor->addSampleClient(&_averager);
            if (!_sorter.isRunning()) _sorter.start();
        }
    }

    /*
     * After the last sensor is connected:
     * 1. SampleIOProcessor::connect(&_avgWrapper);
     * Problem is, how to detect the "last" sensor. Need an init virtual method.
     * CVIOutput must do variable mapping so that the variables get put
     * in right place.
     */

    if (!_resampler) {
        // cerr << "creating resampler" << endl;
        _resampler = new NearestResamplerAtRate(_variables);
        _rsmplrWrapper.reset(new SampleInputWrapper(_resampler));
        _resampler->setRate(_rate);
        /*
         * This method calls addSampleTag on the configured outputs,
         * then then issues a connect request on the outputs.
         */
        SampleIOProcessor::connect(_rsmplrWrapper.get());
    }
    _resampler->connect(&_avgWrapper);

}

void CVIProcessor::attachLVInput(const SampleTag* tag)
    throw(n_u::IOException)
{
    // cerr << "CVIProcessor::attachLVInput: sensor=" <<
      //   _lvSensor->getName() << endl;
    if (getD2ADeviceName().length() > 0) {
        _aout.setDeviceName(getD2ADeviceName());
        _aout.open();
        _numD2A = _aout.getNumOutputs();
#ifdef DEBUG
        cerr << "numD2A=" << _numD2A << endl;
#endif
        for (unsigned int i = 0; i < sizeof(_vouts)/sizeof(_vouts[0]); i++)
            _vouts[i] = -99.0;
    }

    if (getDigIODeviceName().length() > 0) {
        _dout.setDeviceName(getDigIODeviceName());
        _dout.open();
        _numDigout = _dout.getNumOutputs();
        for (unsigned int i = 0; i < sizeof(_douts)/sizeof(_douts[0]); i++)
            _douts[i] = -1;
#ifdef DEBUG
        cerr << "numDigout=" << _numDigout << endl;
#endif
    }
    _lvSampleId = tag->getId();
    _lvSensor->addRawSampleClient(_lvSensor);
    _lvSensor->addSampleClient(this);
}


void CVIProcessor::disconnect(SampleInput* input) throw(n_u::IOException)
{
    if (_resampler) _resampler->disconnect(&_avgWrapper);
    input->removeProcessedSampleClient(&_averager);
    SampleIOProcessor::disconnect(input);
    if (_lvSensor) {
        _lvSensor->removeRawSampleClient(_lvSensor);
        _lvSensor->removeSampleClient(this);
    }
}
 
void CVIProcessor::connected(SampleOutput* orig,
	SampleOutput* output) throw()
{
    SampleIOProcessor::connected(orig,output);
    if (_resampler) _resampler->addSampleClient(output);
}
 
void CVIProcessor::disconnected(SampleOutput* output) throw()
{
    _resampler->removeSampleClient(output);
    SampleOutput* orig = outputMap[output];
    SampleIOProcessor::disconnected(output);
    if (orig) orig->requestConnection(this);
}

bool CVIProcessor::receive(const Sample *insamp) throw()
{
#ifdef DEBUG
    cerr << "CVIProcessor::receive, insamp length=" <<
        insamp->getDataByteLength() << endl;
#endif
    if (insamp->getId() != _lvSampleId) return false;
    if (insamp->getType() != FLOAT_ST) return false;

    const SampleT<float>* fsamp = (const SampleT<float>*) insamp;
    const float* fin = fsamp->getConstDataPtr();

    unsigned int ni = fsamp->getDataLength();
    unsigned int nv = std::min((unsigned int)(sizeof(_vouts)/sizeof(_vouts[0])),_numD2A);
    unsigned int ii = 0;

    vector<int> which;
    vector<float> volts;
#ifdef DEBUG
    cerr << "LV receive " << endl;
    for (unsigned int i = 0; i < ni; i++) cerr << fin[i] << ' ';
    cerr << endl;
#endif

    for (unsigned int i = 0; i < nv && ii < ni; i++,ii++) {
        float f = fin[ii];
        if (fabs(f - _vouts[i]) > 1.e-3) {
            int ni = i;
            /* Temporary flip of 3 and 4 to correct for cross-wired outputs on GV.
             * This wiring issue does not exist on the C130.
             * It is unknown at this point whether the GV rack still needs this switch.
             */
#ifdef FLIP_VOUT_3_4
            if (i == 3) ni = 4;
            else if (i == 4) ni = 3;
#endif
            which.push_back(ni);
            volts.push_back(f);
            _vouts[i] = f;
#ifdef DEBUG
            cerr << "setting VOUT " << i << " to " << f << endl;
#endif
        }
    }
    try {
        if (which.size() > 0) _aout.setVoltages(which,volts);
    }
    catch(n_u::IOException& e) {
        n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s\n",
            _aout.getName().c_str(),e.what());
    }

    unsigned int nd = std::min((unsigned int)(sizeof(_douts)/sizeof(_douts[0])),_numDigout);
    n_u::BitArray dwhich(_numDigout);
    n_u::BitArray dvals(_numDigout);
    for (unsigned int i = 0; i < nd && ii < ni; i++,ii++) {
        int d = (fin[ii] != 0.0); // change to boolean 0 or 1
        if (d != _douts[i]) {
            dwhich.setBit(i,1);
            dvals.setBit(i,d);
            _douts[i] = d;
#ifdef DEBUG
            cerr << "setting DOUT pin " << i <<
                        " to " << d << endl;
#endif
        }
    }
    try {
        if (dwhich.any()) _dout.setOutputs(dwhich,dvals);
    }
    catch(n_u::IOException& e) {
        n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s\n",
            _dout.getName().c_str(),e.what());
    }
    return true;
}

void CVIProcessor::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    SampleIOProcessor::fromDOMElement(node);

    const std::list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi;
    for (pi = params.begin(); pi != params.end(); ++pi) {
        const Parameter* param = *pi;
        const string& pname = param->getName();
        if (pname == "vout") {
                if (param->getLength() != 1)
                    throw n_u::InvalidParameterException(
                        SampleIOProcessor::getName(),"parameter",
                        "bad vout parameter");
                setD2ADeviceName(param->getStringValue(0));
        }
        else if (pname == "dout") {
                if (param->getLength() != 1)
                    throw n_u::InvalidParameterException(
                        SampleIOProcessor::getName(),"parameter",
                        "bad dout parameter");
                setDigIODeviceName(param->getStringValue(0));
        }
    }
}

