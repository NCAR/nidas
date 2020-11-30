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
#include <nidas/core/SampleTracer.h>

#include <iomanip>

#include <cmath>
#include <algorithm>
#include <set>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;
using nidas::util::endlog;
using nidas::util::LogScheme;
using nidas::util::LogContext;
using nidas::util::LogMessage;

/*
 * Whether to compute a time offset of each sample in the sync record,
 * otherwise leave it as 0.0.
 */
#define COMPUTE_OFFSET

/*
 * Whether to log the number of skipped slots.
 */
// #define LOG_SKIPS

/*
 * Whether to log slot skips as they occur.
 */
// #define LOG_ALL_SKIPS

inline
std::string
format_time(dsm_time_t tt, const std::string& fmt = "%Y %m %d %H:%M:%S.%3f")
{
    return n_u::UTime(tt).format(true, fmt);
}

SyncInfo::SyncInfo(dsm_sample_id_t i, float r, SyncRecordSource* srs):
    id(i), rate(r),
    nSlots((int)::ceil(rate)),
    dtUsec((int)rint(USECS_PER_SEC / rate)),
    islot(-1), irec(0),
    varLengths(),
    sampleLength(1),   // initialize to one for timeOffset
    sampleOffset(0),
    variables(), varOffsets(),
    discarded(0), overWritten(0), nskips(0),
    skipped(0), total(0),
    iCheck(0),
    nCheck(std::max(std::max(nSlots, 2), 20)),
    minLate(INT_MAX),
    minDiff(dtUsec),
    secCount(0),
    keepCount((int)(1.0 / (1 - (nSlots-rate)))),
    _srs(srs)
{
    /*
     * keepCount is a way to insert NaNs in sync records
     * with non-integral rates.  If the modulus of secCount % keepCount
     * is non-zero, then the last slot in the record is skipped, leaving
     * a NaN.  Here are the values for some expected rates, including
     * non-integral ARINC rates.
     *
     * rate     keepCount   (secCount % keepCount) ! = 0
     * integral 1           never true, no skips
     * 12.5     2           skip slot every other second
     * 6.25     4           skip slot in 3 out of 4 seconds
     * 3.125    8           skip slot in 7 out of 8
     * 1.5625   1           never true (algorithm breaks down)
     */
}

#ifdef EXPLICIT_SYNCINFO_COPY_ASSIGN
#ifdef DEBUG
unsigned int SyncInfo::ncopy;
#endif
unsigned int SyncInfo::nassign;

SyncInfo::SyncInfo(const SyncInfo& other):
    id(other.id),
    rate(other.rate),
    nSlots(other.nSlots),
    dtUsec(other.dtUsec),
    islot(other.islot),
    irec(other.irec),
    varLengths(other.varLengths),
    sampleLength(other.sampleLength),
    sampleOffset(other.sampleOffset),
    variables(other.variables),
    varOffsets(other.varOffsets),
    discarded(other.discarded),
    overWritten(other.overWritten),
    nskips(other.nskips),
    skipped(other.skipped),
    total(other.total),
    iCheck(other.iCheck),
    nCheck(other.nCheck),
    minLate(other.minLate),
    minDiff(other.minDiff),
    secCount(other.secCount),
    keepCount(other.keepCount),
    _srs(other._srs)
{
#ifdef DEBUG
    cerr << "SyncInfo copy #" << ++ncopy << endl;
#endif
}

SyncInfo& SyncInfo::operator = (const SyncInfo& other)
{
    cerr << "SyncInfo assign #" << ++nassign << endl;
    if (&other == this) return *this;
    this->id = other.id;
    this->rate = other.rate;
    this->nSlots = other.nSlots;
    this->dtUsec = other.dtUsec;
    this->islot = other.islot;
    this->irec = other.irec;
    this->varLengths = other.varLengths;
    this->sampleLength = other.sampleLength;
    this->sampleOffset = other.sampleOffset;
    this->variables = other.variables;
    this->varOffsets = other.varOffsets;
    this->discarded = other.discarded;
    this->overWritten = other.overWritten;
    this->nskips = other.nskips;
    this->skipped = other.skipped;
    this->total = other.total;
    this->iCheck = other.iCheck;
    this->nCheck = other.nCheck;
    this->minLate = other.minLate;
    this->minDiff = other.minDiff;
    this->secCount = other.secCount;
    this->keepCount = other.keepCount;
    this->_srs = other._srs;
    return *this;
}
#endif
void SyncInfo::addVariable(const Variable* var)
{
    size_t vlen = var->getLength();
    varLengths.push_back(vlen);
    varOffsets.push_back(sampleLength);
    sampleLength += vlen * nSlots;
    variables.push_back(var);
}

void SyncInfo::advanceRecord(int ilast)
{
    if (irec == ilast) {
        // last slot for this sample was in the sync record
        // that was just written
        irec = SyncRecordSource::nextRecordIndex(ilast);
        islot = -1;
    }
}

bool SyncInfo::incrementSlot()
{ 
    if (++islot == nSlots) {
        // There is no next sync record.
        if (!_srs->nextRecord(*this)) {
            islot = nSlots - 1;
            return false;
        }
        islot = 0;
    }
    secCount++;
    return checkNonIntRateIncrement();
}

bool SyncInfo::checkNonIntRateIncrement()
{ 
    if (rate == nSlots) return true;

    // the best time to increment is when the last slot time rolls
    // over to the next second:
    //      minDiff + (nSlots-1) * dtUsec > USECS_PER_SEC
    if (islot == nSlots - 1) {
        if (secCount % keepCount) return incrementSlot();
        // no increment, check if we should
        if (minDiff + islot * dtUsec > USECS_PER_SEC) {
            secCount++;
            return incrementSlot();
        }
    }
    return true;
}

bool SyncInfo::decrementSlot()
{ 
    if (islot == 0) {
        if (!_srs->prevRecord(*this)) return false;
        islot = nSlots;
    } 
    islot--;
    secCount--;
    return true;
}

void SyncInfo::computeSlotIndex(const Sample* samp)
{
    islot = _srs->computeSlotIndex(samp, *this);
}

SyncRecordSource::SyncRecordSource():
    _source(false),_syncInfo(), _variables(),
    _syncRecordHeaderSampleTag(),_syncRecordDataSampleTag(),
    _recSize(0),_syncHeaderTime(),_syncTime(),
    _current(0), _halfMaxUsecsPerSample(INT_MIN),
    _syncRecord(),_dataPtr(),_unrecognizedSamples(),
    _headerStream(),
    _aircraft(0),_initialized(false),_unknownSampleType(0),
    _badLaterSamples(0)
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

    // log the discarded, overWritten and, if LOG_SKIPS is defined,
    // the skipped samples for each sample id
    set<dsm_sample_id_t> logIds;
    map<dsm_sample_id_t,SyncInfo>::const_iterator si = _syncInfo.begin();

    for ( ; si != _syncInfo.end(); ++si) {
        if (si->second.discarded + si->second.overWritten > 0)
            logIds.insert(si->first);
#ifdef LOG_SKIPS
        if (si->second.skipped > 0)
            logIds.insert(si->first);
#endif
    }

    set<dsm_sample_id_t>::const_iterator li = logIds.begin();
    for ( ; li != logIds.end(); ++li) {
        dsm_sample_id_t id = *li;
        map<dsm_sample_id_t, SyncInfo>::const_iterator si =
            _syncInfo.find(id); //  we know it will be found
        const SyncInfo& sinfo = si->second;
#ifdef LOG_SKIPS
        ILOG(("SyncRecordSource, sample id %2d,%4d, rate=%7.2f, #discarded=%4u, #overwritten=%4u, #skipped=%4u, #total=%8u", 
            GET_DSM_ID(id), GET_SPS_ID(id), sinfo.rate,
            sinfo.discarded, sinfo.overWritten, sinfo.skipped, sinfo.total));
#else
        ILOG(("SyncRecordSource, sample id %2d,%4d, rate=%7.2f, #discarded=%4u, #overwritten=%4u, #total=%8u", 
            GET_DSM_ID(id), GET_SPS_ID(id), sinfo.rate,
            sinfo.discarded, sinfo.overWritten, sinfo.total));
#endif
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
init()
{
    // Traverse the variables list and lay out the sync record, including
    // all the sample sizes, variable offsets, and rates.  All the
    // non-counter, non-continuous variables have already been excluded by
    // selectVariablesFromSensor().
    //
    //
    cerr << "init()" << endl;
    if (_initialized) return;
    _initialized = true;

    std::list<const Variable*>::iterator vi;
    for (vi = _variables.begin(); vi != _variables.end(); ++vi)
    {
        const Variable* var = *vi;
	const SampleTag* tag = var->getSampleTag();
	dsm_sample_id_t id = tag->getId();
	float rate = tag->getRate();

        if (rate <= 0.0) continue;

        map<dsm_sample_id_t, SyncInfo>::iterator si =
            _syncInfo.find(id);
        if (si == _syncInfo.end()) {
            /* This should be the only place a SyncInfo copy constructor
             * is used.
             * Note that
             *      _syncInfo[id] = SyncInfo(id, rate, srs);
             * requires a no-arg constructor, though one would think
             * it would only require an assignment operator.  We don't 
             * have a no-arg constructor, so so use map::insert(pair<>).
             */
            _syncInfo.insert(
                std::pair<dsm_sample_id_t,SyncInfo>(
                    id, SyncInfo(id, rate, this)));
        }
        si = _syncInfo.find(id);

        SyncInfo& sinfo = si->second;

        sinfo.addVariable(var);

        _halfMaxUsecsPerSample =
            std::max(_halfMaxUsecsPerSample,
                     (int)ceil(USECS_PER_SEC / rate / 2));

        _syncRecordHeaderSampleTag.addVariable(new Variable(*var));
        _syncRecordDataSampleTag.addVariable(new Variable(*var));
    }

    int offset = 0;
    map<dsm_sample_id_t, SyncInfo>::iterator si = _syncInfo.begin();

    for ( ; si != _syncInfo.end(); ++si) {
        SyncInfo& sinfo = si->second;

        sinfo.sampleOffset = offset;
	for (size_t i = 0; i < sinfo.variables.size(); i++) {
            sinfo.varOffsets[i] += offset;
	}
        // cerr << "sinfo.sampleLength=" << sinfo.sampleLength <<
        //     ", offset=" << offset << endl;
	offset += sinfo.sampleLength;
    }
    _recSize = offset;
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

    init();

    source->addSampleClient(this);
}

void SyncRecordSource::disconnect(SampleSource* source) throw()
{
    source->removeSampleClient(this);
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
	    WLOG(("variable name \"%s\" has one or more embedded spaces, "
                  "replacing with \'_\'", varname.c_str()));
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

    // write list of variable names for each rate.
    // There will likely be more than one list for a given rate.
    ost << "rates {" << endl;
    std::map<dsm_sample_id_t, SyncInfo>::const_iterator si = _syncInfo.begin();

    //
    for ( ; si != _syncInfo.end(); ++si) {
        const SyncInfo& sinfo = si->second;
        float rate = sinfo.rate;
	ost << fixed << setprecision(5) << rate << ' ';
	list<const Variable*>::const_iterator vi = sinfo.variables.begin();
	for ( ; vi != sinfo.variables.end(); ++vi) {
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
            static LogContext ilog(LOG_INFO, "calibrations");
            static LogMessage imsg(&ilog);
            if (ilog.active())
            {
                if (lconv) {
                    imsg << var->getName()
                         << " has linear calibration: "
                         << lconv->getIntercept() << " "
                         << lconv->getSlope() << endlog;
                }
                else if (pconv) {
                    std::vector<float> coefs = pconv->getCoefficients();
                    imsg << var->getName() << " has poly calibration: ";
                    for (unsigned int i = 0; i < coefs.size(); ++i)
                        imsg << coefs[i] << " ";
                    imsg << endlog;
                }
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
    // cerr << "Header=" << headstr << endl;

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
    for (int i = 0; i < NSYNCREC; i++) {
        sendSyncRecord();
        _current = nextRecordIndex(_current);
    }
}

void
SyncRecordSource::sendSyncRecord()
{
    assert(_current >= 0 && _current < NSYNCREC);
    if (_syncRecord[_current]) {
        static nidas::util::LogContext lp(LOG_DEBUG);
        if (lp.active())
        {
            dsm_time_t tt = _syncRecord[_current]->getTimeTag();
            lp.log(nidas::util::LogMessage().format("distribute syncRecord, ")
                   << " syncTime=" << tt
                   << " (" << format_time(tt) << ")");
        }
        _source.distribute(_syncRecord[_current]);
        _syncRecord[_current] = 0;
        _syncTime[_current] = LONG_LONG_MIN;
    }
}

void SyncRecordSource::allocateRecord(int irec, dsm_time_t timetag)
{
    if (!_syncRecord[irec]) {
        dsm_time_t syncTime = timetag - (timetag % USECS_PER_SEC);    // beginning of second

        SampleT<double>* sp = _syncRecord[irec] = getSample<double>(_recSize);
        sp->setTimeTag(syncTime);
        sp->setId(SYNC_RECORD_ID);
        _dataPtr[irec] = sp->getDataPtr();
        std::fill(_dataPtr[irec], _dataPtr[irec] + _recSize, doubleNAN);

        _syncTime[irec] = syncTime;

        static nidas::util::LogContext lp(LOG_DEBUG);
        if (lp.active())
        {
            lp.log(nidas::util::LogMessage().format("")
                   << "SyncRecordSource::allocateRecord: timetag="
                   << n_u::UTime(timetag).format(true,"%Y %m %d %H:%M:%S.%3f")
                   << ", syncTime[" << irec << "]="
                   << n_u::UTime(_syncTime[irec]).format(true,
                                                          "%Y %m %d %H:%M:%S.%3f"));
        }
    }
}

int SyncRecordSource::advanceRecord(dsm_time_t timetag)
{
#ifdef DEBUG
    cerr << "before advanceRecord, _syncTime[" << _current << "]=" <<
        n_u::UTime(_syncTime[_current]).format(true,"%H:%M:%S.%4f") <<
        endl;
#endif

    int last = _current;
    sendSyncRecord();
    _current = nextRecordIndex(_current);
    if (!_syncRecord[_current]) allocateRecord(_current,timetag);

    map<dsm_sample_id_t, SyncInfo>::iterator si = _syncInfo.begin();
    for ( ; si != _syncInfo.end(); ++si) {
        si->second.advanceRecord(last);
    }

#ifdef DEBUG
    cerr << "after advanceRecord, _syncTime[" << _current << "]=" <<
        n_u::UTime(_syncTime[_current]).format(true,"%H:%M:%S.%4f") <<
        endl;
#endif
    return _current;
}

template <typename ST>
void
copy_variables_to_record(const Sample* samp, double* dataPtr, int recSize,
                 const vector<size_t>& varOffsets, const vector<size_t>& varLen,
                 int timeIndex)
{
    const ST* fp = (const ST*)samp->getConstVoidDataPtr();
    const ST* ep = fp + samp->getDataLength();

    for (size_t i = 0; i < varLen.size() && fp < ep; i++) {
        size_t outlen = varLen[i];
        size_t inlen = std::min((size_t)(ep-fp), outlen);

        double* dp = dataPtr + varOffsets[i] + outlen * timeIndex;
        if (0)
        {
            DLOG(("varOffsets[") << i << "]=" << varOffsets[i] <<
                 " outlen=" << outlen << " timeIndex=" << timeIndex <<
                 " recSize=" << recSize);
        cerr << " recSize=" << recSize <<
            ", inlen=" << inlen <<
            ", outlen=" << outlen <<
            ", varOffsets[" << i << "]=" << varOffsets[i] <<
            ", timeIndex" << timeIndex << 
            ", recSize=" << recSize << endl;
        }
        assert(dp + inlen <= dataPtr + recSize);
        if (samp->getType() == DOUBLE_ST) {
            memcpy(dp, fp, inlen*sizeof(*dp));
        }
        else {
            for (unsigned int j = 0; j < inlen; j++) dp[j] = fp[j];
        }
        fp += inlen;
    }
}

// static
int SyncRecordSource::nextRecordIndex(int i)
{
    return (i + 1) % NSYNCREC;
}

// static
int SyncRecordSource::prevRecordIndex(int i)
{
    return nextRecordIndex(i);
}

bool SyncRecordSource::prevRecord(SyncInfo& sinfo)
{
    // previous sync rec
    // Can't go earlier than current sync record
    int n = sinfo.getRecordIndex();
    if (n == _current) return false;
    n = prevRecordIndex(n);
    assert(_syncRecord[n]);
    sinfo.decrementRecord();
    return true;
}

bool SyncRecordSource::nextRecord(SyncInfo& sinfo)
{
    int i = sinfo.getRecordIndex();
    // make sure sync record n exists, we needs its syncTime
    assert(_syncRecord[i]);
    if (i != _current) {
        // sinfo.getRecordIndex() is already pointing to the second sync record,
        sinfo.overWritten++;
        return false;
    }
    dsm_time_t tnext = _syncTime[i] + USECS_PER_SEC;
    int n = nextRecordIndex(i);
    allocateRecord(n, tnext);
    if (_syncTime[n] - _syncTime[i] != USECS_PER_SEC) {
        nidas::util::LogContext lcerror(LOG_ERR);
        log(lcerror, "bad syncTimes", sinfo);
    }
    assert(_syncTime[n] - _syncTime[i] == USECS_PER_SEC);
    sinfo.incrementRecord();
    return true;
}

int SyncRecordSource::computeFirstOffset(const Sample* samp,
        const SyncInfo& sinfo)
{
#ifdef COMPUTE_OFFSET
    int off = (int)(samp->getTimeTag() % USECS_PER_SEC % sinfo.dtUsec);
    /* For a non-integral sampleRate, where
     * nSlots=ceil(sampleRate) is rounded
     * up, then (dtUsec * nSlots) is larger
     * than a second. We don't want the offset to be more than
     * USECS_PER_SEC - (dtUsec * (nSlots-1)
     */
    off = std::min(off,
            USECS_PER_SEC - (sinfo.dtUsec * (sinfo.nSlots-1)));
    return off;
#else
    return 0;
#endif
}

int SyncRecordSource::computeSlotIndex(const Sample* samp,
        SyncInfo& sinfo)
{
    int n = sinfo.getRecordIndex();
    if (!_syncRecord[n]) {
        nidas::util::LogContext lcerror(LOG_ERR);
        log(lcerror, "!_syncRecord[n]", samp, sinfo);
    }
    assert (_syncRecord[n]); 

    if (samp->getTimeTag() >= _syncTime[n] + USECS_PER_SEC)
        nextRecord(sinfo);

    int ni = (int)std::max(
        (int)(samp->getTimeTag() - _syncTime[sinfo.getRecordIndex()]) / sinfo.dtUsec,0);

    return ni;
}
    
bool SyncRecordSource::checkTime(const Sample* samp,
        SyncInfo& sinfo, SampleTracer& stracer, LogContext& lc,
        int warn_times_interval)
{
    bool ret = true;
    slog(stracer, "before checkTime: ", samp, sinfo);

    if (sinfo.getSlotIndex() == 0) sinfo.minDiff = sinfo.dtUsec;

    // slot time of next sample in sync record
    dsm_time_t tn = _syncTime[sinfo.getRecordIndex()] + sinfo.getSlotIndex() * sinfo.dtUsec;

    int tdiff = samp->getTimeTag() - tn;
    if (tdiff < 0) {    // sample time tag is earlier than slot time
        sinfo.minLate = INT_MAX;
        sinfo.iCheck = 0;
        sinfo.minDiff = 0;
        if (-tdiff > sinfo.dtUsec / 2) {
            // timetag is earlier than slot time by more than 1/2 sample dt.
            // This happens if the configured rate of the sample is less than the
            // actual reporting rate. It also happens when samples come in 
            // bursts, even if the configured rate is close to the reporting rate.
            if (!sinfo.decrementSlot()) {
                if (!(sinfo.discarded++ % warn_times_interval)) {
                    ostringstream ost;
                    ost << "timetag < slot time=" <<
                        format_time(tn,"%H:%M:%S.%4f") <<
                        " by more than " << 0.5 << "*dt, discarding";
                    log(lc, ost.str(), samp, sinfo);
                }
                ret = false;
            }
        }
    }
    else if (tdiff > std::min(NSLOT_LIMIT * sinfo.dtUsec, USECS_PER_SEC)) {
        // timetag is later than slot time by more than NSLOT_LIMIT
        // sample dt. Need to skip some slots.  Compute index of this
        // sample into the current sync record. This is the result of
        // latency gaps or actual gaps, or if the configured rate of
        // the sample is more than the actual reporting rate.

        int ilast = sinfo.getSlotIndex();
        sinfo.computeSlotIndex(samp);

        int di = sinfo.getSlotIndex() - ilast;
        if (di < 0) di += sinfo.nSlots;
        sinfo.skipped += di;

#ifdef LOG_ALL_SKIPS
        if (!(sinfo.nskips++ % warn_times_interval)) {
            ostringstream ost;
            ost << "timetag > slot time=" <<
                format_time(tn,"%H:%M:%S.%4f") <<
                " by more than " << NSLOT_LIMIT << "*dt, skipping " <<
                di << " slots";
            log(lc, ost.str(), samp, sinfo);
        }
#endif

        ret = true;
    }
    else {
        int ni = tdiff / sinfo.dtUsec;
        sinfo.minDiff = std::min(sinfo.minDiff, tdiff);
        if (ni == 0) {
            sinfo.iCheck = 0;
            sinfo.minLate = INT_MAX;
        }
        else if (++sinfo.iCheck < sinfo.nCheck) {
            sinfo.minLate = std::min(ni, sinfo.minLate);
        }
        else {
            // more than nCheck samples all received more than
            // one dt late
            ostringstream ost;
            ost << "checkTime, minLate=" << sinfo.minLate << ": ";
            slog(stracer, ost.str(), samp, sinfo);

            while (sinfo.minLate--) {
                if (!sinfo.incrementSlot()) break;
            }
            sinfo.iCheck = 0;
            sinfo.minLate = INT_MAX;
            ret = true;
        }
    }
    slog(stracer, "after checkTime: ", samp, sinfo);
    return ret;
}

void SyncRecordSource::slog(SampleTracer& stracer,const string& msg,
        const Sample* samp, const SyncInfo& sinfo)
{
    if (stracer.active(samp))
    {
        stracer.msg(samp, msg) << ": syncTimes=" <<
            (_syncTime[0] < 0 ? "--:--:--.-" :
            format_time(_syncTime[0],"%H:%M:%S.%1f")) << ", " <<
            (_syncTime[1] < 0 ? "--:--:--.-" :
            format_time(_syncTime[1],"%H:%M:%S.%1f")) <<
            ", _current=" << _current << 
            ", irec=" << sinfo.getRecordIndex() <<
            ", islot=" << sinfo.getSlotIndex() <<
            ", minLate=" << (sinfo.minLate == INT_MAX ? -1 : sinfo.minLate) <<
            ", minDiff=" << (double)sinfo.minDiff / USECS_PER_SEC <<
            ", dt=" << setprecision(3) << 1/sinfo.rate <<
            ", nSlots=" << sinfo.nSlots <<
            ", keep=" << sinfo.keepCount <<
            ", #sec=" << sinfo.secCount <<
            endlog;
    }
}

void SyncRecordSource::log(LogContext& lc, const string& msg,
        const Sample* samp, const SyncInfo& sinfo)
{
    if (lc.active())
    {
        dsm_sample_id_t id = samp->getId();
        n_u::LogMessage lmsg;
        lmsg << msg << ": tt=" <<
            format_time(samp->getTimeTag(), "%F %T.%4f") <<
            ", id=" << GET_DSM_ID(id) << "," << GET_SPS_ID(id) <<
            ", syncTimes=" << 
            (_syncTime[0] < 0 ? "--:--:--.-" :
                format_time(_syncTime[0],"%H:%M:%S.%1f")) << ", " <<
            (_syncTime[1] < 0 ? "--:--:--.-" :
            format_time(_syncTime[1],"%H:%M:%S.%1f")) <<
            ", _current=" << _current << 
            ", irec=" << sinfo.getRecordIndex() <<
            ", islot=" << sinfo.getSlotIndex() <<
            ", minLate=" << (sinfo.minLate == INT_MAX ? -1 : sinfo.minLate) <<
            ", minDiff=" << (double)sinfo.minDiff / USECS_PER_SEC <<
            ", dt=" << setprecision(3) << 1/sinfo.rate <<
            ", nSlots=" << sinfo.nSlots <<
            ", keep=" << sinfo.keepCount <<
            ", #sec=" << sinfo.secCount;
        lc.log(lmsg);
    }
}

void SyncRecordSource::log(LogContext& lc, const string& msg,
        const SyncInfo& sinfo)
{
    if (lc.active())
    {
        n_u::LogMessage lmsg;
        lmsg << msg <<
            ": syncTimes=" << 
            (_syncTime[0] < 0 ? "--:--:--.-" :
                format_time(_syncTime[0],"%H:%M:%S.%1f")) << ", " <<
            (_syncTime[1] < 0 ? "--:--:--.-" :
            format_time(_syncTime[1],"%H:%M:%S.%1f")) <<
            ", _current=" << _current << 
            ", irec=" << sinfo.getRecordIndex() <<
            ", islot=" << sinfo.getSlotIndex() <<
            ", minLate=" << (sinfo.minLate == INT_MAX ? -1 : sinfo.minLate) <<
            ", minDiff=" << (double)sinfo.minDiff / USECS_PER_SEC <<
            ", dt=" << setprecision(3) << 1/sinfo.rate <<
            ", nSlots=" << sinfo.nSlots <<
            ", keep=" << sinfo.keepCount <<
            ", #sec=" << sinfo.secCount;
        lc.log(lmsg);
    }
}

bool SyncRecordSource::receive(const Sample* samp) throw()
{
    static int warn_times_interval =
        LogScheme::current().getParameterT("sync_warn_times_interval",
                                           1000);

    /*
     * To enable the SampleTracer for a given sample id, add these arguments
     * to sync_server:
     *  --logconfig enable,level=verbose,function=SyncRecordSource::receive
     *  --logparam trace_samples=20,141
     *  --logfields level,message
     */
    static SampleTracer stracer(LOG_VERBOSE);
    static nidas::util::LogContext lcerror(LOG_ERR);
    static nidas::util::LogContext lcwarn(LOG_WARNING);
    static nidas::util::LogContext lcinfo(LOG_INFO);

    dsm_time_t tt = samp->getTimeTag();
    dsm_sample_id_t sampleId = samp->getId();

    map<dsm_sample_id_t, SyncInfo>::iterator si =
        _syncInfo.find(sampleId);
    if (si == _syncInfo.end()) return false;
    SyncInfo& sinfo = si->second;

    sinfo.total++;

    // first time
    if (!_syncRecord[_current]) allocateRecord(_current, samp->getTimeTag());

    // Screen bad times.
    if (tt < _syncTime[_current]) {
        if (!(sinfo.discarded++ % warn_times_interval))
	    log(lcwarn, "timetag < syncTime, dropping", samp, sinfo);
	return false;
    }

    if (tt >= _syncTime[_current] + 2 * USECS_PER_SEC &&
        _syncTime[_current] > LONG_LONG_MIN)
    {
        if (!(_badLaterSamples++ % warn_times_interval))
	    log(lcwarn, "timetag > syncTime + 2s", samp, sinfo);
    }

    /*
     * If we have a time tag greater than the end of the current
     * sync record, plus half the maximum sample delta-T, then the
     * current sync record is ready to ship.
     */
    dsm_time_t triggertime = _syncTime[_current] + USECS_PER_SEC +
        std::max(_halfMaxUsecsPerSample, 250 * USECS_PER_MSEC);
    if (tt >= triggertime) advanceRecord(tt);

    // First of this sample encounters, or the last sample was in
    // previous sync record which was written out. Restart
    if (sinfo.getSlotIndex() < 0) sinfo.computeSlotIndex(samp);
    // normal case increment, report if you can't.
    else if (!sinfo.incrementSlot()) {
        slog(stracer, "no next sync record",
                samp, sinfo);
        sinfo.overWritten++;
    }

    slog(stracer, "SyncRecordSource::receive: ", samp, sinfo);

    if (!checkTime(samp, sinfo, stracer, lcinfo, warn_times_interval))
        return false;    // discard sample

    if (sinfo.getRecordIndex() < 0 || sinfo.getRecordIndex() >= NSYNCREC)
        log(lcerror, "bad record index: ", samp, sinfo);
    assert(sinfo.getRecordIndex() >= 0 && sinfo.getRecordIndex() < NSYNCREC);

    if (sinfo.getSlotIndex() < 0 || sinfo.getSlotIndex() >= sinfo.nSlots)
        log(lcerror, "bad slot index: ", samp, sinfo);

    assert(sinfo.getSlotIndex() >= 0 && sinfo.getSlotIndex() < sinfo.nSlots);

    assert(sinfo.sampleOffset < _recSize);

    // store offset into sync record
    _dataPtr[sinfo.getRecordIndex()][sinfo.sampleOffset] = sinfo.minDiff;

    switch (samp->getType()) {

    case UINT32_ST:
        copy_variables_to_record<uint32_t>(samp, _dataPtr[sinfo.getRecordIndex()],
		_recSize, sinfo.varOffsets, sinfo.varLengths, sinfo.getSlotIndex());
	break;
    case FLOAT_ST:
        copy_variables_to_record<float>(samp, _dataPtr[sinfo.getRecordIndex()],
                _recSize, sinfo.varOffsets, sinfo.varLengths, sinfo.getSlotIndex());
	break;
    case DOUBLE_ST:
        copy_variables_to_record<double>(samp, _dataPtr[sinfo.getRecordIndex()],
                _recSize, sinfo.varOffsets, sinfo.varLengths, sinfo.getSlotIndex());
	break;
    default:
	if (!(_unknownSampleType++ % 1000))
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"sample id %d is not a float, double or uint32 type",sampleId);
        break;
    }
    slog(stracer, "returning: ", samp, sinfo);
    return true;
}

