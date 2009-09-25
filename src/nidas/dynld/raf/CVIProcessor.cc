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
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,CVIProcessor)

CVIProcessor::CVIProcessor(): SampleIOProcessor(false),
    _outputSampleTag(0),_rate(0.0),_lvSampleId(0),
    _numD2A(0),_numDigout(0)
{
    setName("CVIProcessor");
}

CVIProcessor::~CVIProcessor()
{
    std::set<SampleOutput*>::const_iterator oi = _connectedOutputs.begin();
    for ( ; oi != _connectedOutputs.end(); ++oi) {
        SampleOutput* output = * oi;
        _averager.removeSampleClient(output);

        try {
            output->finish();
            output->close();
        }
        catch (const n_u::IOException& ioe) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                "%s: error closing %s: %s",
                getName().c_str(),output->getName().c_str(),ioe.what());
        }

        SampleOutput* orig = output->getOriginal();
        if (orig != output)
            delete output;
    }
}

void CVIProcessor::addRequestedSampleTag(SampleTag* tag)
	throw(n_u::InvalidParameterException)
{
    if (getSampleTags().size() > 1)
        throw n_u::InvalidParameterException("CVIProcessor","sample","cannot have more than one sample");

    _outputSampleTag = tag;

    VariableIterator vi = tag->getVariableIterator();
    for ( ; vi.hasNext(); ) {
        const Variable* var = vi.next();
        _varMatched.push_back(false);
        _averager.addVariable(var);
    }
    if (tag->getRate() <= 0.0) {
        ostringstream ost;
        ost << "invalid rate: " << _rate;
        throw n_u::InvalidParameterException("CVIProcessor","sample",ost.str());
    }
    _averager.setAveragePeriodSecs(1.0/tag->getRate());

    // SampleIOProcessor will delete
    SampleIOProcessor::addRequestedSampleTag(tag);
    addSampleTag(_outputSampleTag);
}

void CVIProcessor::connect(SampleSource* source)
    throw(n_u::InvalidParameterException,n_u::IOException)
{
    /*
     * In the typical usage on a DSM, this connection will
     * be from the SamplePipeline.
     */
    source = source->getProcessedSampleSource();
    assert(source);

    _connectionMutex.lock();

    // on first SampleSource connection, request output connections.
    if (_connectedSources.size() == 0) {
        const list<SampleOutput*>& outputs = getOutputs();
        list<SampleOutput*>::const_iterator oi = outputs.begin();
        for ( ; oi != outputs.end(); ++oi) {
            SampleOutput* output = *oi;
            // some SampleOutputs want to know what they are getting
            output->addSourceSampleTags(getSampleTags());
            SampleOutputRequestThread::getInstance()->addConnectRequest(output,this,0);
        }
    }
    _connectedSources.insert(source);
    _connectionMutex.unlock();

    DSMSensor* sensor = 0;

    SampleTagIterator inti = source->getSampleTagIterator();
    for ( ; inti.hasNext(); ) {
        const SampleTag* intag = inti.next();
        dsm_sample_id_t sensorId = intag->getId() - intag->getSampleId();
        // find sensor for this sample
        sensor = Project::getInstance()->findSensor(sensorId);

#ifdef DEBUG
        if (sensor) cerr << "CVIProcessor::connect sensor=" <<
            sensor->getName() << " (" << GET_DSM_ID(sensorId) << ',' << 
            GET_SHORT_ID(sensorId) << ')' << endl;
        else cerr << "CVIProcessor no sensor" << endl;
#endif

        // can throw IOException
        if (sensor && _lvSampleId == 0 && dynamic_cast<CVI_LV_Input*>(sensor))
                attachLVInput(source,intag);
        // sensor->setApplyVariableConversions(true);
    }

    _averager.connect(source);
}

void CVIProcessor::disconnect(SampleSource* source) throw()
{
    source = source->getProcessedSampleSource();

    _connectionMutex.lock();
    _connectedSources.erase(source);
    _connectionMutex.unlock();

    _averager.disconnect(source);
    _averager.finish();
    source->removeSampleClient(this);
}
 
void CVIProcessor::attachLVInput(SampleSource* source, const SampleTag* tag)
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
    source->addSampleClientForTag(this,tag);
}

void CVIProcessor::connect(SampleOutput* output) throw()
{
    _connectionMutex.lock();
    _averager.addSampleClient(output);
    _connectedOutputs.insert(output);
    _connectionMutex.unlock();
}
 
void CVIProcessor::disconnect(SampleOutput* output) throw()
{
    _averager.removeSampleClient(output);

    _connectionMutex.lock();
    _connectedOutputs.erase(output);
    _connectionMutex.unlock();

    try {
        output->finish();
        output->close();
    }
    catch (const n_u::IOException& ioe) {
        n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: error closing %s: %s",
            getName().c_str(),output->getName().c_str(),ioe.what());
    }

    SampleOutput* orig = output->getOriginal();
    if (orig != output)
        SampleOutputRequestThread::getInstance()->addDeleteRequest(output);

    // reschedule a request for the original output.
    SampleOutputRequestThread::getInstance()->addConnectRequest(orig,this,10);
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

