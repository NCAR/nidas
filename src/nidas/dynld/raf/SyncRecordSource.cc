// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

// #define DEBUG

#include "SyncRecordSource.h"
#include "Aircraft.h"
#include <nidas/core/SampleInput.h>
#include <nidas/core/Project.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/Variable.h>
#include <nidas/core/CalFile.h>
#include <nidas/util/UTime.h>

#include <nidas/util/Logger.h>
#include <nidas/core/Version.h>

#include <iomanip>

#include <cmath>
#include <algorithm>

// #define DEBUG

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

SyncRecordSource::SyncRecordSource():
    _source(false),_varsByIndex(),_sampleIndices(),
    _intSamplesPerSec(),_rates(),_usecsPerSample(),
    _halfMaxUsecsPerSample(INT_MIN),
    _offsetUsec(),_sampleLengths(),_sampleOffsets(),
    _varOffsets(),_varLengths(),_numVars(),_variables(),
    _syncRecordHeaderSampleTag(),_syncRecordDataSampleTag(),
    _recSize(0),_syncHeaderTime(),_syncTime(),
    _current(0),
    _syncRecord(),_dataPtr(),_unrecognizedSamples(),
    _headerStream(), _badLaterTimes(0),_badEarlierTimes(0),
    _aircraft(0),_initialized(false),_unknownSampleType(0)
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
    _syncTime[0] = LONG_LONG_MIN;
    _syncTime[1] = LONG_LONG_MIN;
}

SyncRecordSource::~SyncRecordSource()
{
    if (_syncRecord[0]) _syncRecord[0]->freeReference();
    if (_syncRecord[1]) _syncRecord[1]->freeReference();

    map<dsm_sample_id_t,int*>::const_iterator vi;
    for (unsigned int i = 0; i < _varOffsets.size(); i++) {
	delete [] _varOffsets[i];
	delete [] _varLengths[i];
    }
}



void
SyncRecordSource::
selectVariablesFromSensor(DSMSensor* sensor, 
                          std::list<const Variable*>& variables)
{
    list<SampleTag*> tags = sensor->getSampleTags();
    list<SampleTag*>::const_iterator ti;
    map<dsm_sample_id_t, int> idmap;

    for (ti = tags.begin(); ti != tags.end(); ++ti)
    {
	const SampleTag* tag = *ti;

	if (!tag->isProcessed()) continue;

	dsm_sample_id_t sampleId = tag->getId();

	const vector<const Variable*>& vars = tag->getVariables();

	// skip samples with one non-continuous, non-counter variable
	if (vars.size() == 1) {
	    Variable::type_t vt = vars.front()->getType();
	    if (vt != Variable::CONTINUOUS && vt != Variable::COUNTER)
                continue;
	}

        map<dsm_sample_id_t, int>::const_iterator gi = idmap.find(sampleId);

        // check if this sample id has already been seen (shouldn't happen)
        if (gi != idmap.end()) {
	    n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"Sample id %d,%d is not unique",
                    GET_DSM_ID(sampleId),GET_SPS_ID(sampleId));
            continue;
        }
        idmap[sampleId] = variables.size();

	vector<const Variable*>::const_iterator vi;
	for (vi = vars.begin(); vi != vars.end(); ++vi)
        {
	    const Variable* var = *vi;
	    Variable::type_t vt = var->getType();
	    if (vt == Variable::CONTINUOUS || vt == Variable::COUNTER)
            {
		variables.push_back(var);
	    }
	}
    }
}

void
SyncRecordSource::
selectVariablesFromProject(Project* project, 
                           std::list<const Variable*>& variables)
{
    // Traverse the project sensors and samples selecting the variables
    // which qualify for inclusion in a sync record.

    set<DSMSensor*> sensors;
    SensorIterator ti = project->getSensorIterator();
    while (ti.hasNext())
    {
        DSMSensor* sensor = ti.next();
        if (sensors.insert(sensor).second)
        {
            selectVariablesFromSensor(sensor, variables);
        }
    }
}


void
SyncRecordSource::
layoutSyncRecord()
{
    // Traverse the variables list and lay out the sync record, including
    // all the sample sizes, variable offsets, and rates.  All the
    // non-counter, non-continuous variables have already been excluded by
    // selectVariablesFromSensor().

    const SampleTag* lasttag = 0;
    int iv = 0;
    std::list<const Variable*>::iterator vi;
    for (vi = _variables.begin(); vi != _variables.end(); ++vi)
    {
        const Variable* var = *vi;
	const SampleTag* tag = var->getSampleTag();
	dsm_sample_id_t sampleId = tag->getId();
	float rate = tag->getRate();
        int nvars = tag->getVariables().size();
	int sampleIndex = _varsByIndex.size();

        if (lasttag == tag)
        {
            --sampleIndex;
            ++iv;
        }
        else
        {
            iv = 0;
            lasttag = tag;
            _sampleIndices[sampleId] = sampleIndex;
            _varsByIndex.push_back(list<const Variable*>());
            _sampleLengths.push_back(0);
            _sampleOffsets.push_back(0);
            _rates.push_back(rate);
            _usecsPerSample.push_back((int)rint(USECS_PER_SEC / rate));
            _halfMaxUsecsPerSample =
                std::max(_halfMaxUsecsPerSample,
                         (int)ceil(USECS_PER_SEC / rate / 2));

            _intSamplesPerSec.push_back((int)ceil(rate));
            _offsetUsec[0].push_back(-1);
            _offsetUsec[1].push_back(-1);

            int* varOffset = new int[nvars];
            _varOffsets.push_back(varOffset);

            size_t* varLen = new size_t[nvars];
            _varLengths.push_back(varLen);

            _numVars.push_back(nvars);
        }
        size_t vlen = var->getLength();
        _varLengths[sampleIndex][iv] = vlen;
        _varOffsets[sampleIndex][iv] = -1;
        _varOffsets[sampleIndex][iv] = _sampleLengths[sampleIndex];
        _sampleLengths[sampleIndex] += vlen*_intSamplesPerSec[sampleIndex];
        _varsByIndex[sampleIndex].push_back(var);
        _syncRecordHeaderSampleTag.addVariable(new Variable(*var));
        _syncRecordDataSampleTag.addVariable(new Variable(*var));
    }
}


void SyncRecordSource::connect(SampleSource* source) throw()
{
    DLOG(("SyncRecordSource::connect() ")
         << "setting up variables and laying out sync record.");
    source = source->getProcessedSampleSource();

    Project* project = Project::getInstance();
    // nimbus needs to know the aircraft
    if (!_aircraft)
    {
        _aircraft = Aircraft::getAircraft(project);
    }

    selectVariablesFromProject(project, _variables);
    layoutSyncRecord();

    source->addSampleClient(this);
    init();
}

void SyncRecordSource::disconnect(SampleSource* source) throw()
{
    source->removeSampleClient(this);
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
	offset += _sampleLengths[si] + 1;   // add one for timeOffset
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
    std::streamsize oldprecision = ost.precision();

    ost << "project  \"" << _aircraft->getProject()->getName() << '"' << endl;
    ost << "aircraft \"" << _aircraft->getTailNumber() << '"' << endl;

    string fn = _aircraft->getProject()->getFlightName();
    if (fn.length() == 0) fn = "unknown_flight";
    ost << "flight \"" << fn << '"' << endl;
    ost << "software_version \"" << Version::getSoftwareVersion() << '"' << endl;

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
            ost << setprecision(16);
            if (lconv) {
                ost << lconv->getIntercept() << ' ' <<
                        lconv->getSlope();
            }
            else {
                const Polynomial* pconv = dynamic_cast<const Polynomial*>(conv);
                if (pconv) {
                    const std::vector<float>& coefs = pconv->getCoefficients();
                    for (unsigned int i = 0; i < coefs.size(); i++)
                        ost << coefs[i] << ' ';
                }
            }
            ost.precision(oldprecision);
            ost << " \"" << conv->getUnits() << "\" ";
            const CalFile* cfile = conv->getCalFile();
            if (cfile) {
                ost << "file=\"" << cfile->getFile() <<
                    "\" path=\"" << cfile->getPath() << "\"";
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


void SyncRecordSource::preLoadCalibrations(dsm_time_t sampleTime) throw()
{
    _syncHeaderTime = sampleTime;
    ILOG(("initialized sync header time to ")
         << n_u::UTime(_syncHeaderTime).format(true,"%Y %m %d %H:%M:%S.%3f")
         << ", pre-loading calibrations...");
    list<const Variable*>::iterator vi;
    for (vi = _variables.begin(); vi != _variables.end(); ++vi) {
        Variable* var = const_cast<Variable*>(*vi);
	VariableConverter* conv = var->getConverter();
	if (conv) {
            conv->readCalFile(sampleTime);
            Linear* lconv = dynamic_cast<Linear*>(conv);
            Polynomial* pconv = dynamic_cast<Polynomial*>(conv);
            if (lconv) {
                ILOG(("") << var->getName()
                     << " has linear calibration: "
                     << lconv->getIntercept() << " "
                     << lconv->getSlope());
            }
            else if (pconv) {
                std::vector<float> coefs = pconv->getCoefficients();
                std::ostringstream msg;
                msg << var->getName() << " has poly calibration: ";
                for (unsigned int i = 0; i < coefs.size(); ++i)
                    msg << coefs[i] << " ";
                ILOG(("") << msg.str());
            }
	}
    }
    ILOG(("Calibration pre-load done."));
}



void SyncRecordSource::sendSyncHeader() throw()
{
    _headerStream.str("");	// initialize header to empty string

    // reset output to the general type (not fixed, not scientific)
    _headerStream.setf(ios_base::fmtflags(0),ios_base::floatfield);
    // 6 digits of precision.
    _headerStream.precision(6);

    createHeader(_headerStream);
    string headstr = _headerStream.str();

    SampleT<char>* headerRec = getSample<char>(headstr.length()+1);
    headerRec->setTimeTag(_syncHeaderTime);

    DLOG(("SyncRecordSource::sendSyncHeader timetag=")
         << headerRec->getTimeTag());
    DLOG(("sync header=\n") << headstr);

    headerRec->setId(SYNC_RECORD_HEADER_ID);
    strcpy(headerRec->getDataPtr(), headstr.c_str());

    _source.distribute(headerRec);
}

void SyncRecordSource::flush() throw()
{
    DLOG(("SyncRecordSource::flush()"));
    for (int i = 0; i < NSYNCREC; i++) sendSyncRecord();
}

void
SyncRecordSource::sendSyncRecord()
{
    if (_syncRecord[_current]) {
        static nidas::util::LogContext lp(LOG_DEBUG);
        if (lp.active()) 
        {
            lp.log(nidas::util::LogMessage().format("distribute syncRecord, ")
                   << " syncTime=" << _syncRecord[_current]->getTimeTag());
        }
        _source.distribute(_syncRecord[_current]);
        _syncRecord[_current] = 0;
        std::fill(_offsetUsec[_current].begin(), _offsetUsec[_current].end(), -1);
    }
    _current = (_current + 1) % NSYNCREC;
}

void SyncRecordSource::allocateRecord(int isync, dsm_time_t timetag)
{
    dsm_time_t syncTime = timetag - (timetag % USECS_PER_SEC);    // beginning of second

    SampleT<double>* sp = _syncRecord[isync] = getSample<double>(_recSize);
    sp->setTimeTag(syncTime);
    sp->setId(SYNC_RECORD_ID);
    _dataPtr[isync] = sp->getDataPtr();
    std::fill(_dataPtr[isync], _dataPtr[isync] + _recSize, doubleNAN);
    std::fill(_offsetUsec[isync].begin(), _offsetUsec[isync].end(), -1);

    _syncTime[isync] = syncTime;

    static nidas::util::LogContext lp(LOG_DEBUG);
    if (lp.active()) 
    {
        lp.log(nidas::util::LogMessage().format("")
               << "SyncRecordSource::allocateRecord: timetag="
               << n_u::UTime(timetag).format(true,"%Y %m %d %H:%M:%S.%3f")
               << ", syncTime[" << isync << "]="
               << n_u::UTime(_syncTime[isync]).format(true,
                                                      "%Y %m %d %H:%M:%S.%3f"));
    }

}

int SyncRecordSource::advanceRecord(dsm_time_t timetag)
{
    sendSyncRecord();
    if (!_syncRecord[_current]) allocateRecord(_current,timetag);
    return _current;
}

template <typename ST>
void
copy_variables_to_record(const Sample* samp, double* dataPtr, int recSize,
                         int* varOffset, size_t* varLen, size_t numVar,
                         int timeIndex)
{
    const ST* fp = (const ST*)samp->getConstVoidDataPtr();
    const ST* ep = fp + samp->getDataLength();

    for (size_t i = 0; i < numVar && fp < ep; i++) {
        size_t outlen = varLen[i];
        size_t inlen = std::min((size_t)(ep-fp), outlen);

        if (varOffset[i] >= 0) {
            double* dp = dataPtr + varOffset[i] + 1 + outlen * timeIndex;
            if (0)
            {
                DLOG(("varOffset[") << i << "]=" << varOffset[i] <<
                     " outlen=" << outlen << " timeIndex=" << timeIndex <<
                     " recSize=" << recSize);
            }
            assert(dp + outlen <= dataPtr + recSize);
            // XXX
            // This is a little dangerous because it assumes if the types
            // have the same size then they are the same type with the same
            // representation in memory.  It fails for integer types with the
            // same size as a double, but so far I guess that is never true
            // on nidas platforms.
            // XXX
            if (sizeof(*fp) != sizeof(*dp))
                for (unsigned int j = 0; j < inlen; j++) dp[j] = fp[j];
            else
                memcpy(dp, fp, inlen*sizeof(*dp));
        }
        fp += inlen;
    }
}

int
SyncRecordSource::
sampleIndexFromId(dsm_sample_id_t sampleId)
{
    map<dsm_sample_id_t, int>::const_iterator gi;
    gi = _sampleIndices.find(sampleId);
    if (gi == _sampleIndices.end()) {
        _unrecognizedSamples++;
        DLOG(("unrecognizedSample, id=") << GET_DSM_ID(sampleId)
             << ',' << GET_SPS_ID(sampleId));
	return -1;
    }
    return gi->second;
}

bool SyncRecordSource::receive(const Sample* samp) throw()
{
    dsm_time_t tt = samp->getTimeTag();
    dsm_sample_id_t sampleId = samp->getId();

    int sampleIndex = sampleIndexFromId(sampleId);
    if (sampleIndex < 0)
        return false;

    assert(sampleIndex < (signed)_usecsPerSample.size());
    int usecsPerSamp = _usecsPerSample[sampleIndex];

    int isync = _current;
    if (!_syncRecord[isync]) allocateRecord(isync,tt);

#ifdef DEBUG
    cerr << "SyncRecordSource::receive: " << GET_DSM_ID(sampleId) << ',' <<
        GET_SPS_ID(sampleId) <<
        ",tt=" << n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ", syncTime[" << isync << "]=" <<
        n_u::UTime(_syncTime[isync]).format(true,"%Y %m %d %H:%M:%S.%3f") << endl;
#endif

    // Screen bad times.
    // 
    // It looks like this code identifies problem samples when they precede
    // the current syncTime or when they jump forward in time more than two
    // seconds.  However, the _syncTime is always adjusted to line up with
    // the second containing the latest sample time (see sendSyncRecord()),
    // so if a time truly is "future bad" then it will be followed by lots
    // of "early bad" samples which will be skipped.  I'm not sure what the
    // intention might have been here, so I may have messed it up when I
    // introduced the sendSyncRecord() method.  Either way, calling
    // sendSyncRecord() guarantees that there is a _syncRecord for the
    // current _syncTime and that this newest sample aligns somewhere
    // inside the _syncRecord, and thus that call must happen after the
    // comparisons to the current _syncTime to find problem times.
    if (tt < _syncTime[isync]) {
        if (!(_badEarlierTimes++ % 1000))
	    WLOG(("SyncRecordSource: sample timetag (%s) < syncTime (%s) by %f sec, dsm=%d, id=%d\n",
                n_u::UTime(tt).format(true,"%F %T.%4f").c_str(),
                n_u::UTime(_syncTime[isync]).format(true,"%F %T.%4f").c_str(),
		(double)(_syncTime[isync]-tt)/USECS_PER_SEC,
                GET_DSM_ID(sampleId),GET_SHORT_ID(sampleId)));
	return false;
    }

    if (tt >= _syncTime[isync] + 2 * USECS_PER_SEC && _syncTime[isync] > LONG_LONG_MIN) {
        if (!(_badLaterTimes++ % 1))
	    WLOG(("SyncRecordSource: sample timetag (%s) > syncTime (%s) by %f sec, dsm=%d, id=%d\n",
                n_u::UTime(tt).format(true,"%F %T.%4f").c_str(),
                n_u::UTime(_syncTime[isync]).format(true,"%F %T.%4f").c_str(),
		(double)(tt-_syncTime[isync])/USECS_PER_SEC,
                GET_DSM_ID(sampleId),GET_SHORT_ID(sampleId)));
    }

    /*
     * If we have a time tag greater than the start time of the
     * second sync record (whose time is _syncTime[isync] + USECS_PER_SEC)
     * plus half the maximum sample delta-T, then the first sync record
     * is ready to ship.
     */
    dsm_time_t triggertime = _syncTime[isync] + USECS_PER_SEC;
    triggertime += _halfMaxUsecsPerSample;
    if (tt >= triggertime) {
        static nidas::util::LogContext lp(LOG_DEBUG);
        if (lp.active()) 
        {
            n_u::LogMessage msg;
            msg << "prior to SyncRecordSource::advanceRecord: tt="
                << n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%3f")
                << ", syncTime[" << isync << "]="
                << n_u::UTime(_syncTime[isync]).format(true,
                                                       "%Y %m %d %H:%M:%S.%3f")
                << ", triggerTime="
                << n_u::UTime(triggertime).format(true,"%Y %m %d %H:%M:%S.%3f");
            lp.log(msg);
        }
        isync = advanceRecord(tt);
    }

    /* check if belongs in next sync record */
    if (tt >= _syncTime[isync] + USECS_PER_SEC + usecsPerSamp/2) {
        isync = (isync + 1) % NSYNCREC;
        if (!_syncRecord[isync]) allocateRecord(isync,tt);
    }

    int intSamplesPerSec = _intSamplesPerSec[sampleIndex];

    /* if a sample has not yet been stored in a sync record.
     * the offsetUsec will be -1
     */
    int offsetUsec = _offsetUsec[isync][sampleIndex];

    /*
     * Compute time index into samples's row.
     */
    int timeIndex;
    if (offsetUsec < 0) // first sample of this id for the record
        timeIndex = (int)(tt - _syncTime[isync]) / usecsPerSamp;
    else
        timeIndex = (int)(tt - _syncTime[isync] - offsetUsec + usecsPerSamp/2) / usecsPerSamp;

    /*
     * If times are out of order and a multi-second forward time jump occured
     * previously, tt here could be less than _syncTime[isync] and then timeIndex < 0.
     */
    if (timeIndex < 0) {
        if (!(_badEarlierTimes++ % 1000))
	    WLOG(("SyncRecordSource: sample timetag (%s) < syncTime (%s) by %f sec, dsm=%d, id=%d\n",
                n_u::UTime(tt).format(true,"%F %T.%4f").c_str(),
                n_u::UTime(_syncTime[isync]).format(true,"%F %T.%4f").c_str(),
		(double)(_syncTime[isync]-tt)/USECS_PER_SEC,
                GET_DSM_ID(sampleId),GET_SHORT_ID(sampleId)));
	return false;
    }

#ifdef DEBUG
    if (GET_DSM_ID(sampleId) == 19 && GET_SPS_ID(sampleId) == 4081)
        cerr << "SyncRecordSource: " << GET_DSM_ID(sampleId) << ',' << GET_SPS_ID(sampleId) <<
            ",tt=" << n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%3f") <<
            ",_current=" << _current <<
        ", syncTime[" << 0 << "]=" <<
        n_u::UTime(_syncTime[0]).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ", syncTime[" << 1 << "]=" <<
        n_u::UTime(_syncTime[1]).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ",_offsetUsec[0][sampleIndex] =" << _offsetUsec[0][sampleIndex] <<
        ",_offsetUsec[1][sampleIndex] =" << _offsetUsec[1][sampleIndex] <<
        ", offsetUsec=" << offsetUsec <<
        ", timeIndex=" << timeIndex <<
        ",_halfMaxUsecsPerSample=" << _halfMaxUsecsPerSample <<
        endl;
#endif

    if (timeIndex >= intSamplesPerSec) {
        /* belongs in next sync record */
        int is = (isync + 1) % NSYNCREC;
        if (!_syncRecord[is])
            allocateRecord(is,std::max(tt,_syncTime[isync]+USECS_PER_SEC));
        isync = is;
        offsetUsec = _offsetUsec[isync][sampleIndex];

        if (offsetUsec < 0) // first sample of this id for the record
            timeIndex = (int)(tt - _syncTime[isync]) / usecsPerSamp;
        else
            timeIndex = (int)(tt - _syncTime[isync] - offsetUsec + usecsPerSamp/2) / usecsPerSamp;
    }

    if (offsetUsec < 0 || timeIndex == 0) {
        /*
         * First instance of this sample in current sync record.
         * Compute the variable's time offset into the second from
         * the first sample received each second.
         */
        offsetUsec = std::max((int)(tt - _syncTime[isync]) % usecsPerSamp,0);

        // store offset into sync record
        _offsetUsec[isync][sampleIndex] = offsetUsec;
        int offsetIndex = _sampleOffsets[sampleIndex];
        _dataPtr[isync][offsetIndex] = offsetUsec;
    }

#ifdef DEBUG
    if (GET_DSM_ID(sampleId) == 19 && GET_SPS_ID(sampleId) == 4081)
        cerr << "SyncRecordSource done: " << GET_DSM_ID(sampleId) << ',' << GET_SPS_ID(sampleId) <<
        ", tt=" << n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ", syncTime[" << 0 << "]=" <<
            n_u::UTime(_syncTime[0]).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ", syncTime[" << 1 << "]=" <<
            n_u::UTime(_syncTime[1]).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ", _current=" << _current <<
        ", isync=" << isync <<
        ", usecsPerSamp=" << usecsPerSamp <<
        ", offsetUsec=" << offsetUsec <<
        ",_offsetUsec[0][sampleIndex] =" << _offsetUsec[0][sampleIndex] <<
        ",_offsetUsec[1][sampleIndex] =" << _offsetUsec[1][sampleIndex] <<
        ", timeIndex=" << timeIndex <<
        ", offsetIndex=" << _sampleOffsets[sampleIndex] <<  endl;
#endif

    if (timeIndex >= intSamplesPerSec) {
        ELOG(("SyncRecordSource, timeIndex >= N: id=") << GET_DSM_ID(sampleId) << ',' << GET_SPS_ID(sampleId) <<
        ", tt=" << n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ", syncTime[" << 0 << "]=" <<
            n_u::UTime(_syncTime[0]).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ", syncTime[" << 1 << "]=" <<
            n_u::UTime(_syncTime[1]).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ", _current=" << _current <<
        ", isync=" << isync <<
        ", usecsPerSamp=" << usecsPerSamp <<
        ", offsetUsec=" << offsetUsec <<
        ",_offsetUsec[0][sampleIndex] =" << _offsetUsec[0][sampleIndex] <<
        ",_offsetUsec[1][sampleIndex] =" << _offsetUsec[1][sampleIndex] <<
        ", timeIndex=" << timeIndex <<
        ", offsetIndex=" << _sampleOffsets[sampleIndex]);
    }

    assert(timeIndex >= 0 && timeIndex < intSamplesPerSec);

    int* varOffset = _varOffsets[sampleIndex];
    assert(varOffset);
    size_t* varLen = _varLengths[sampleIndex];
    assert(varLen);
    size_t numVar = _numVars[sampleIndex];
    assert(numVar);

#ifdef DEBUG
    if (GET_DSM_ID(sampleId) == 19 && GET_SPS_ID(sampleId)  == 155) {
        cerr << "SyncRecordSource: " << GET_DSM_ID(sampleId) << ',' << GET_SPS_ID(sampleId) <<
        ", tt=" << n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ", syncTime[" << isync << "]=" <<
            n_u::UTime(_syncTime[isync]).format(true,"%Y %m %d %H:%M:%S.%3f") <<
        ", usecsPerSamp=" << usecsPerSamp <<
        ", offsetUsec=" << offsetUsec <<
        ", timeIndex=" << timeIndex <<
        ", offsetIndex=" << _sampleOffsets[sampleIndex] << 
        ", numVar=" << numVar <<
        ", varOffset[0]=" << varOffset[0] <<
        ", varLen[0]=" << varLen[0] << endl;
    }
#endif
	
    switch (samp->getType()) {

    case UINT32_ST:
        copy_variables_to_record<uint32_t>(samp, _dataPtr[isync], _recSize,
                                           varOffset, varLen, numVar,
                                           timeIndex);
	break;
    case FLOAT_ST:
        copy_variables_to_record<float>(samp, _dataPtr[isync], _recSize,
                                        varOffset, varLen, numVar,
                                        timeIndex);
	break;
    case DOUBLE_ST:
        copy_variables_to_record<double>(samp, _dataPtr[isync], _recSize,
                                         varOffset, varLen, numVar,
                                         timeIndex);
	break;
    default:
	if (!(_unknownSampleType++ % 1000)) 
	    n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"sample id %d is not a float or double type",sampleId);

        break;
    }
    return true;
}

