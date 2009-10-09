/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/dynld/raf/SyncRecordSource.h>
#include <nidas/core/SampleInput.h>
#include <nidas/dynld/raf/Aircraft.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

#include <iomanip>

#include <cmath>

// #define DEBUG

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

SyncRecordSource::SyncRecordSource():
        _source(false),_syncRecord(0),_badTimes(0),_aircraft(0),
	_initialized(false),_unknownSampleType(0)
{
    _syncRecordHeaderSampleTag.setDSMId(0);
    _syncRecordHeaderSampleTag.setSensorId(0);
    _syncRecordHeaderSampleTag.setSampleId(SYNC_RECORD_HEADER_ID);
    _syncRecordHeaderSampleTag.setRate(0.0);
    addSampleTag(&_syncRecordHeaderSampleTag);

    _syncRecordDataSampleTag.setDSMId(0);
    _syncRecordDataSampleTag.setSensorId(0);
    _syncRecordDataSampleTag.setSampleId(SYNC_RECORD_ID);
    _syncRecordDataSampleTag.setRate(1.0);
    addSampleTag(&_syncRecordDataSampleTag);
}

SyncRecordSource::~SyncRecordSource()
{
    if (_syncRecord) _syncRecord->freeReference();

    map<dsm_sample_id_t,int*>::const_iterator vi;
    for (unsigned int i = 0; i < _varOffsets.size(); i++) {
	delete [] _varOffsets[i];
	delete [] _varLengths[i];
    }
}

void SyncRecordSource::connect(SampleSource* source) throw()
{
    source = source->getProcessedSampleSource();

    // make a copy of source's SampleTags collection
    list<const SampleTag*> itags = source->getSampleTags();
    list<const SampleTag*>::const_iterator si = itags.begin();

    for ( ; si != itags.end(); ++si) {
	const SampleTag* stag = *si;

	// nimbus needs to know the aircraft
	if (!_aircraft) {
	    const DSMConfig* dsm =
		    Project::getInstance()->findDSM(stag->getDSMId());
	    const Site* site = dsm->getSite();
	    const Aircraft* acft = dynamic_cast<const Aircraft*>(site);
	    _aircraft = acft;
	}
        DSMSensor* sensor =
            Project::getInstance()->findSensor(stag);
        if (!sensor) continue;
        addSensor(sensor);
    }
    source->addSampleClient(this);
    init();
}

void SyncRecordSource::disconnect(SampleSource* source) throw()
{
    source->removeSampleClient(this);
}

void SyncRecordSource::addSensor(DSMSensor* sensor) throw()
{

    if (!_sensorSet.insert(sensor).second) return;

    list<const SampleTag*> tags = sensor->getSampleTags();

    list<const SampleTag*>::const_iterator ti;
    for (ti = tags.begin(); ti != tags.end(); ++ti) {
	const SampleTag* tag = *ti;

	if (!tag->isProcessed()) continue;

	dsm_sample_id_t sampleId = tag->getId();
	float rate = tag->getRate();

	const vector<const Variable*>& vars = tag->getVariables();

	// skip samples with one non-continuous, non-counter variable
	if (vars.size() == 1) {
	    Variable::type_t vt = vars.front()->getType();
	    if (vt != Variable::CONTINUOUS && vt != Variable::COUNTER)
		    continue;
	}

        map<dsm_sample_id_t, int>::const_iterator gi =
            _sampleIndices.find(sampleId);

        // check if this sample id has already been seen (shouldn't happen)
        if (gi != _sampleIndices.end()) {
	    n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"Sample id %d,%d is not unique",
                    GET_DSM_ID(sampleId),GET_SPS_ID(sampleId));
            continue;
        }
        
	int sampleIndex = _varsByIndex.size();

        _sampleIndices[sampleId] = sampleIndex;

        _varsByIndex.push_back(list<const Variable*>());

        _sampleLengths.push_back(0);
        _sampleOffsets.push_back(0);

        _rates.push_back(rate);
        _usecsPerSample.push_back((int)rint(USECS_PER_SEC / rate));
        _samplesPerSec.push_back((int)ceil(rate));

        int* varOffset = new int[vars.size()];
        _varOffsets.push_back(varOffset);

        size_t* varLen = new size_t[vars.size()];
        _varLengths.push_back(varLen);

        _numVars.push_back(vars.size());

	vector<const Variable*>::const_iterator vi;
	int iv;
	for (vi = vars.begin(),iv=0; vi != vars.end(); ++vi,iv++) {
	    const Variable* var = *vi;
	    size_t vlen = var->getLength();
	    varLen[iv] = vlen;

	    Variable::type_t vt = var->getType();
	    varOffset[iv] = -1;
	    if (vt == Variable::CONTINUOUS || vt == Variable::COUNTER) {
		varOffset[iv] = _sampleLengths[sampleIndex];
		_sampleLengths[sampleIndex] += vlen * _samplesPerSec[sampleIndex];
		_varsByIndex[sampleIndex].push_back(var);
		_variables.push_back(var);
	    }
	    _syncRecordHeaderSampleTag.addVariable(new Variable(*var));
	    _syncRecordDataSampleTag.addVariable(new Variable(*var));
	}
    }
}

void SyncRecordSource::init()
{

    if (_initialized) return;
    _initialized = true;

    int offset = 0;
    // iterate over the sample indices, setting the offsets of each
    // sample and variable into the sync record.
    for (unsigned int si = 0; si < _varsByIndex.size(); si++) {
	_sampleOffsets[si] = offset;
	offset += _sampleLengths[si] + 1;
	for (size_t i = 0; i < _numVars[si]; i++) {
	    if (_varOffsets[si][i] >= 0)
		_varOffsets[si][i] += _sampleOffsets[si];
	}
    }
    _recSize = offset;
}

/* local utility function to replace one character in a string
 * with another.
 */
namespace {
void replace_util(string& str,char c1, char c2) {
    for (string::size_type bi; (bi = str.find(c1)) != string::npos;)
	str.replace(bi,1,1,c2);
}
}

void SyncRecordSource::createHeader(ostream& ost) throw()
{

    ost << "project  " << _aircraft->getProject()->getName() << endl;
    ost << "aircraft " << _aircraft->getTailNumber() << endl;

    string fn = _aircraft->getProject()->getFlightName();
    if (fn.length() == 0) fn = "unknown_flight";
    ost << "flight " << fn << endl;

    ost << '#' << endl;     // # indicates end of keyed value section

    // write variable fields.

    // variable type abbreviations:
    //		n=normal, continuous
    //		c=counter
    //		t=clock
    //		o=other
    const char vtypes[] = { 'n','c','t','o' };

    ost << "variables {" << endl;
    list<const Variable*>::const_iterator vi;
    for (vi = _variables.begin(); vi != _variables.end(); ++vi) {
        const Variable* var = *vi;

	string varname = var->getName();
	if (varname.find(' ') != string::npos) {
	    n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"variable name \"%s\" has one or more embedded spaces, replacing with \'_\'",
		varname.c_str());
	    replace_util(varname,' ','_');
	}

	char vtypeabbr = 'o';
	unsigned int iv = (int)var->getType();
	if (iv < sizeof(vtypes)/sizeof(vtypes[0]))
		vtypeabbr = vtypes[iv];

	ost << varname << ' ' <<
		vtypeabbr << ' ' <<
		var->getLength() << ' ' <<
		"\"" << var->getUnits() << "\" " <<
		"\"" << var->getLongName() << "\" ";
	const VariableConverter* conv = var->getConverter();
	if (conv) {
	    const Linear* lconv = dynamic_cast<const Linear*>(conv);
	    if (lconv) {
	        ost << lconv->getIntercept() << ' ' <<
			lconv->getSlope() << " \"" << lconv->getUnits() << "\"";
	    }
	    else {
		const Polynomial* pconv = dynamic_cast<const Polynomial*>(conv);
		if (pconv) {
		    const std::vector<float>& coefs = pconv->getCoefficients();
		    for (unsigned int i = 0; i < coefs.size(); i++)
			ost << coefs[i] << ' ';
		    ost << " \"" << pconv->getUnits() << "\"";
		}
	    }
	}
	else ost << " \"" << var->getUnits() << "\"";
	ost << ';' << endl;
    }
    ost << "}" << endl;

    // write rate entries
    ost << "rates {" << endl;
    for (unsigned int sampleIndex = 0; sampleIndex < _varsByIndex.size(); sampleIndex++) {
        float rate = _rates[sampleIndex];
	ost << fixed << setprecision(2) << rate << ' ';
	list<const Variable*>::const_iterator vi;
	for (vi = _varsByIndex[sampleIndex].begin();
		vi != _varsByIndex[sampleIndex].end(); ++vi) {
	    const Variable* var = *vi;
	    string varname = var->getName();
	    replace_util(varname,' ','_');
	    ost << varname << ' ';
	}
	ost << ';' << endl;
    }
    ost << "}" << endl;

}

void SyncRecordSource::allocateRecord(dsm_time_t timetag)
{
    _syncRecord = getSample<float>(_recSize);
    _syncRecord->setTimeTag(timetag);
    _syncRecord->setId(SYNC_RECORD_ID);
    _floatPtr = _syncRecord->getDataPtr();
    for (int i = 0; i < _recSize; i++) _floatPtr[i] = floatNAN;
}

void SyncRecordSource::sendHeader(dsm_time_t thead) throw()
{
    _headerStream.str("");	// initialize header to empty string

    // reset output to the general type (not fixed, not scientific)
    _headerStream.setf(ios_base::fmtflags(0),ios_base::floatfield);
    // 6 digits of precision.
    _headerStream.precision(6);

    createHeader(_headerStream);
    string headstr = _headerStream.str();

    SampleT<char>* headerRec = getSample<char>(headstr.length()+1);
    headerRec->setTimeTag(thead);

#ifdef DEBUG
    cerr << "SyncRecordSource::sendHeader timetag=" << headerRec->getTimeTag() << endl;
    cerr << "sync header=" << endl << headstr << endl;
#endif

    headerRec->setId(SYNC_RECORD_HEADER_ID);
    strcpy(headerRec->getDataPtr(),headstr.c_str());

    _source.distribute(headerRec);

}

void SyncRecordSource::finish() throw()
{
    cerr << "SyncRecordSource::finish" << endl;
    if (_syncRecord) {
	_source.distribute(_syncRecord);
	_syncRecord = 0;
	_syncTime += USECS_PER_SEC;
    }
    flush();
}

void SyncRecordSource::flush() throw()
{
    _source.flush();
}

bool SyncRecordSource::receive(const Sample* samp) throw()
{
#ifdef DEBUG
    static int nsamps;
    if (!(nsamps++ % 100)) cerr <<
    	"SyncRecordSource, nsamps=" << nsamps << endl;
#endif

    dsm_time_t tt = samp->getTimeTag();
    dsm_sample_id_t sampleId = samp->getId();

    if (!_syncRecord) {
        _syncTime = tt - (tt % USECS_PER_SEC);
	allocateRecord(_syncTime);
    }
	
    // screen bad times
    if (tt < _syncTime) {
        if (!(_badTimes++ % 1000))
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"SyncRecordSource: sample timetag < syncTime by %f sec, dsm=%d, id=%d\n",
		(double)(_syncTime-tt)/USECS_PER_SEC,GET_DSM_ID(sampleId),GET_SHORT_ID(sampleId));
	return false;
    }
    if (tt >= _syncTime + 2 * USECS_PER_SEC) {
        if (!(_badTimes++ % 1000))
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"SyncRecordSource: sample timetag > syncTime by %f sec, dsm=%d, id=%d\n",
		(double)(tt-_syncTime)/USECS_PER_SEC,GET_DSM_ID(sampleId),GET_SHORT_ID(sampleId));
    }
    if (tt >= _syncTime + USECS_PER_SEC) {
#ifdef DEBUG
	cerr << "distribute syncRecord, tt=" <<
		tt << " syncTime=" << _syncTime << endl;
#endif

        if (_syncRecord) {
            _source.distribute(_syncRecord);
            _syncRecord = 0;
            _syncTime += USECS_PER_SEC;
        }
	if (tt >= _syncTime + USECS_PER_SEC) {	// leap forward
	    _badTimes++;
	    _syncTime = tt - (tt % USECS_PER_SEC);
	}
	allocateRecord(_syncTime);
    }

    map<dsm_sample_id_t, int>::const_iterator gi =  _sampleIndices.find(sampleId);
    if (gi == _sampleIndices.end()) {
        _unrecognizedSamples++;
#ifdef DEBUG
	cerr << "unrecognizedSample, id=" << GET_DSM_ID(sampleId) << ',' << GET_SPS_ID(sampleId) << endl;
#endif
	return false;
    }
        
    int sampleIndex = gi->second;

    assert(sampleIndex < (signed)_usecsPerSample.size());
    int usecsPerSamp = _usecsPerSample[sampleIndex];

    int* varOffset = _varOffsets[sampleIndex];
    assert(varOffset);
    size_t* varLen = _varLengths[sampleIndex];
    assert(varLen);
    size_t numVar = _numVars[sampleIndex];
    assert(numVar);

    // rate	usec/sample
    //	1000	1000
    //	100	10000
    //	50	20000
    //  12.5	80000
    //  10	100000
    //  8       125000
    //	1	1000000
    //

    int timeIndex = (tt - _syncTime) / usecsPerSamp;
    assert(timeIndex < _samplesPerSec[sampleIndex]);

    int offsetIndex = _sampleOffsets[sampleIndex];
    if (::isnan(_floatPtr[offsetIndex])) _floatPtr[offsetIndex] =
	    tt - _syncTime - (timeIndex * usecsPerSamp);

    switch (samp->getType()) {

    case FLOAT_ST:
	{
	    const float* fp = (const float*)samp->getConstVoidDataPtr();
	    const float* ep = fp + samp->getDataLength();

	    for (size_t i = 0; i < numVar && fp < ep; i++) {
	        size_t outlen = varLen[i];
	        size_t inlen = std::min((size_t)(ep-fp),outlen);

		if (varOffset[i] >= 0) {
		    float* dp = _floatPtr + varOffset[i] + 1 +
		    	outlen * timeIndex;
		    assert(dp + outlen <= _floatPtr + _recSize);
		    memcpy(dp,fp,inlen*sizeof(float));
		}
		fp += inlen;
	    }
	}
	break;
    default:
	if (!(_unknownSampleType++ % 1000)) 
	    n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"sample id %d is not a float type",sampleId);

        break;
    }
    return true;
}

