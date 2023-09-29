// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include "StatisticsProcessor.h"
#include <nidas/core/Project.h>
#include <nidas/core/Variable.h>
#include <nidas/core/Site.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/util.h>

#include <set>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;
using nidas::util::LogContext;
using nidas::util::LogMessage;
using nidas::util::Logger;

bool stats_log_variable(const Variable* variable)
{
    // use a static to setup the name to match once, since it's unlikely to
    // change after the first call.
    static std::string log_variable_name =
        Logger::getScheme().getParameterT("stats_log_variable_name",
                                          std::string("Vbatt"));
    static bool default_match = (log_variable_name == ".");
    return default_match ||
        variable->getName().find(log_variable_name) != string::npos;
}


StatisticsCruncher::StatisticsCruncher(StatisticsProcessor* proc,
                                       const SampleTag* stag,
                                       statisticsType stype,
                                       std::string cntsName,
                                       bool himom):
        _proc(proc),
        _source(false),
        _reqTag(*stag),
        // Make sure _reqTag is cast to a (const SampleTag&) here, not
        // a (const SampleTag). Otherwise a temporary copy of a SampleTag is
        // created, and reqVariables is initialized from the temporary copy's
        // vector of pointers to Variables, which become invalid when the
        // temporary is deleted.
        _reqVariables(((const SampleTag&)_reqTag).getVariables()),
        _ninvars(_reqVariables.size()),
	_countsName(cntsName),
	_numpoints(_countsName.length() > 0),_periodUsecs(0),
	_crossTerms(false),
	_resampler(0),
	_statsType(stype),_splitVarNames(),
        _leadCommon(),_commonSuffix(),_outSample(),_nOutVar(0),
	_outlen(0),
	_tout(LONG_LONG_MIN),_sampleMap(),
	_xMin(0),_xMax(0),_xSum(0),_xySum(0),_xyzSum(0),_x4Sum(0),
	_nSamples(0),_triComb(0),
	_nsum(0),_ncov(0),_ntri(0),_n1mom(0),_n2mom(0),_n3mom (0),_n4mom(0),_ntot(0),
        _higherMoments(himom),
        _site(0),_station(-1),
        _startTime(LONG_LONG_MIN),_endTime(LONG_LONG_MAX),
        _fillGaps(false)
{
    assert(_ninvars > 0);

    switch(_statsType) {
    case STATS_UNKNOWN:
    case STATS_MINIMUM:
    case STATS_MAXIMUM:
    case STATS_MEAN:
    case STATS_VAR:
    case STATS_SUM:
        _crossTerms = false;
	break;
    case STATS_COV:
    case STATS_TRIVAR:
    case STATS_PRUNEDTRIVAR:
    case STATS_FLUX:
    case STATS_RFLUX:
    case STATS_SFLUX:
        _crossTerms = true;
	_numpoints = true;
        break;
    case STATS_WINDDIR:
        _crossTerms = true;
	break;
    }


    // A StatisticsCruncher is created when the StatisticsProcessor finds
    // a match for the first variable in a statistics group with a SampleSource.
    // StatisticsProcessor then sets that site on all the requested variables.
    _site = _reqVariables[0]->getSite();
    {
        static LogContext lp(LOG_VERBOSE);
        if (lp.active() && stats_log_variable(_reqVariables.front())) {
            LogMessage msg(&lp, "StatisticsCruncher ctor: ");
            msg << "dsmid=" << _reqTag.getDSMId() << ", reqVars= ";
            for (unsigned int i = 0; i < _reqVariables.size(); i++) {
                msg << _reqVariables[i]->getName() << ':'
                    << _reqVariables[i]->getSite()->getName()
                    << '(' << _reqVariables[i]->getStation() << "), ";
            }
        }
    }
    _periodUsecs = (dsm_time_t)rint(MSECS_PER_SEC / stag->getRate()) *
    	USECS_PER_MSEC;

    _outSample.setSampleId(stag->getSpSId());
    _outSample.setDSMId(stag->getDSMId());
    _outSample.setRate(stag->getRate());

    addSampleTag(&_outSample);
}

StatisticsCruncher::~StatisticsCruncher()
{
    map<dsm_sample_id_t,sampleInfo >::iterator vmi;
    for (vmi = _sampleMap.begin(); vmi != _sampleMap.end(); ++vmi) {
	struct sampleInfo& sinfo = vmi->second;
	vector<unsigned int*>& vindices = sinfo.varIndices;
	for (unsigned int iv = 0; iv < vindices.size(); iv++)
	    delete [] vindices[iv];
    }

    delete [] _xSum;
    delete [] _xMin;
    delete [] _xMax;
    if (_xySum) delete [] _xySum[0];
    delete [] _xySum;
    delete [] _xyzSum;
    delete [] _x4Sum;
    delete [] _nSamples;

    if (_triComb) {
	for (unsigned int i=0; i < _ntri; i++) delete [] _triComb[i];
	delete [] _triComb;
    }

    delete _resampler;
}

void StatisticsCruncher::setStartTime(const nidas::util::UTime& val) 
{
    _startTime = val;
    if (_startTime.toUsecs() != LONG_LONG_MIN) 
        _tout =  _startTime.toUsecs() + _periodUsecs - _startTime.toUsecs() % _periodUsecs;
}



std::map<std::string, StatisticsCruncher::statisticsType> StatsStrings
{
    { "minimum", StatisticsCruncher::STATS_MINIMUM },
    { "min", StatisticsCruncher::STATS_MINIMUM },
    { "maximum", StatisticsCruncher::STATS_MAXIMUM },
    { "max", StatisticsCruncher::STATS_MAXIMUM },
    { "mean", StatisticsCruncher::STATS_MEAN },
    { "sum", StatisticsCruncher::STATS_SUM },
    { "variance", StatisticsCruncher::STATS_VAR },
    { "var", StatisticsCruncher::STATS_VAR },
    { "covariance", StatisticsCruncher::STATS_COV },
    { "flux", StatisticsCruncher::STATS_FLUX },
    { "reducedflux", StatisticsCruncher::STATS_RFLUX },
    { "scalarflux", StatisticsCruncher::STATS_SFLUX },
    { "trivar", StatisticsCruncher::STATS_TRIVAR },
    { "prunedtrivar", StatisticsCruncher::STATS_PRUNEDTRIVAR },
    { "winddir", StatisticsCruncher::STATS_WINDDIR }
};


/* static */
StatisticsCruncher::statisticsType
StatisticsCruncher::getStatisticsType(const string& type)
{
    auto it = StatsStrings.find(type);
    if (it != StatsStrings.end())
    {
        return it->second;
    }
    throw n_u::InvalidParameterException(
        "StatisticsCruncher", "unrecognized type type", type);
}


/* static */
const std::string&
StatisticsCruncher::getStatisticsString(statisticsType stype)
{
    static std::string none{"none"};
    for (auto& it: StatsStrings)
    {
        if (it.second == stype)
            return it.first;
    }
    return none;
}


void StatisticsCruncher::connect(SampleSource* source)
{
    assert(_outSample.getVariables().size() == 0);
    assert (!_resampler);

    set<dsm_sample_id_t> matchingTags; 

    bool needResampler = false;
    for (unsigned int i = 0; i < _reqVariables.size(); i++) {
        bool match = false;

        SampleTagIterator inti = source->getSampleTagIterator();
        for ( ; inti.hasNext(); ) {
            const SampleTag* intag = inti.next();
            // loop over variables in this input, checking
            // for a match against one of my variable names.

            VariableIterator vi = intag->getVariableIterator();
            for ( ; vi.hasNext(); ) {
                const Variable* var = vi.next();
                if (*var == *_reqVariables[i]) {
                    static LogContext lp(LOG_VERBOSE);
                    if (lp.active() && stats_log_variable(_reqVariables[0]))
                    {
                        lp.log() << "StatisticsCruncher::connect, var=" << var->getName() <<
                            "(" << var->getSite()->getName() << ")" <<
                            "(" << var->getStation() << ")" <<
                            ", reqVar=" << _reqVariables[i]->getName() <<
                            "(" << _reqVariables[i]->getSite()->getName() << ")" <<
                            "(" << _reqVariables[i]->getStation() << ")" <<
                            ", match=" << (*var == *_reqVariables[i]);
                    }
                    _reqTag.getVariable(i) = *var;
                    match = true;
                    matchingTags.insert(intag->getId());
                    break;  // no need to check other variables in this
                            // sample against _reqVariables[i]

                }
            }
        }
        if (!match) {
            ostringstream ost;
            ost << _reqVariables[i]->getName() <<
                "(" << _reqVariables[i]->getSite()->getName() << "," <<
                _reqVariables[i]->getStation() << ")";
            WLOG(("StatisticsCruncher: no match for variable: ") << ost.str());
        }
    }
    VLOG(("") << _reqVariables[0]->getName() << " " <<
         "(" << _reqVariables[0]->getSite()->getName() << ")" <<
         "(" << _reqVariables[0]->getStation() << 
         ", matchingTags.size()=" << matchingTags.size());
    if (matchingTags.empty()) return;

    // If there are cross terms in requested statistics, and
    // multiple input tags match, then need to resample
    if (_crossTerms && matchingTags.size() > 1)
        needResampler = true;

    if (needResampler && !_resampler) {
        static LogContext lp(LOG_VERBOSE);
        if (lp.active() && stats_log_variable(_reqVariables[0])) {
            lp.log() << "StatisticsCruncher::connect: reqVars= ";
            for (unsigned int i = 0; i < _reqVariables.size(); i++)
                cerr << _reqVariables[i]->getName() << "(" <<
                    _reqVariables[i]->getStation() << "), ";
        }
        _resampler = new NearestResampler(_reqVariables);
    }

    if (_resampler) {
        _resampler->connect(source);
        attach(_resampler);
    }
    else attach(source);

    // create output variable
    createCombinations();
    initStats();
    zeroStats();

    ostringstream ost;
    ost << "outSample=";
    for (unsigned int i = 0; i < _outSample.getVariables().size(); i++) {
        if (i > 0) ost << ", ";
        ost << _outSample.getVariable(i).getName() + "(" << _outSample.getVariable(i).getStation() << ")";
    }
    DLOG(("StatisticsCruncher: ") << ost.str() << ", outSample.getStation=" << _outSample.getStation());

}

void StatisticsCruncher::disconnect(SampleSource* source) throw()
{
    if (_resampler) _resampler->disconnect(source);
    else source->removeSampleClient(this);
}

void StatisticsCruncher::attach(SampleSource* source)
{
    // In order to improve support for the ISFS Wisard motes, where
    // the same variable can appear in more than one sample 
    // (for example if a sensor's input is moved between motes), this code
    // allows matching of a variable from more than one input sample.

    vector <bool> varMatches(_ninvars);
    unsigned int nmatches = 0;
    int sourceStation = -1;

    // make a copy of source's SampleTags collection.
    list<const SampleTag*> intags = source->getSampleTags();

    list<const SampleTag*>::const_iterator inti = intags.begin();
    for ( ; inti != intags.end(); ++inti ) {
	const SampleTag* intag = *inti;
	dsm_sample_id_t id = intag->getId();

	map<dsm_sample_id_t,sampleInfo >::iterator vmi =
	    _sampleMap.find(id);

	if (vmi != _sampleMap.end()) {
            ostringstream ost;
            ost << "StatisticsCruncher: multiple connections for sample id=" <<
		GET_DSM_ID(id) << ',' << GET_SPS_ID(id);
            throw n_u::InvalidParameterException(ost.str());
        }

	struct sampleInfo sinfo;
	struct sampleInfo* sptr = &sinfo;

        // If the input source is a NearestResampler, the sample 
        // will contain a weights variable, indicating how
        // many non-NaNs are in each sample. If our sample
        // contains cross terms we must have a complete input sample
        // with no NaNs. Having a weights variable reduces
        // the overhead, so we don't have to pre-scan the data.

        // currently this code will not work on variables with
        // length > 1

	sptr->weightsIndex = UINT_MAX;

	vector<unsigned int*>& varIndices = sptr->varIndices;
	for (unsigned int rv = 0; rv < _reqVariables.size(); rv++) {
            Variable& reqvar = _reqTag.getVariable(rv);

	    // loop over variables in the sample tag from the source, checking
	    // for a match against one of my variable names.

	    VariableIterator vi = intag->getVariableIterator();
	    for ( ; vi.hasNext(); ) {
		const Variable* invar = vi.next();

                // index of 0th value of variable in its sample data array.
                unsigned int vindex = intag->getDataIndex(invar);

		if (invar->getType() == Variable::WEIGHT) {
		    sptr->weightsIndex = vindex;
		    continue;
		}
		VLOG(("") << "invar=" << invar->getName() <<
              " reqvar=" << reqvar.getName());

		// variable match
		if (*invar == reqvar) {
                    if (!varMatches[rv]) {
                        nmatches++;
                        varMatches[rv] = true;
                    }
                    if (_station < 0) {
                        int vstn = invar->getStation();
                        if (vstn >= 0) {
                            if (sourceStation < 0) _station = sourceStation = vstn; 
                            else if (vstn != sourceStation) _station = 0;
                        }
                    }
		    // Variable matched by name and site, copy other stuff
		    // reqvar = *invar;
                    VLOG(("") << "StatisticsCruncher::attach, id=" <<
                        _outSample.getDSMId() << ',' << _outSample.getSpSId() <<
                        ", match, invar=" << invar->getName() <<
                        " (" << invar->getSampleTag()->getDSMId() << ',' <<
                        invar->getSampleTag()->getSpSId() << "), " <<
                        " stn=" << invar->getStation() <<
                        ", reqvar=" << reqvar.getName() << 
                        " (" << reqvar.getSampleTag()->getDSMId() << ',' <<
                        reqvar.getSampleTag()->getSpSId() << "), " <<
                        " stn=" << reqvar.getStation() <<
                        ", sourceStation=" << sourceStation <<
                        ", _station=" << _station);

		    unsigned int j;
		    // paranoid check that this variable hasn't been added
		    for (j = 0; j < varIndices.size(); j++)
			if ((unsigned)varIndices[j][1] == rv) break;

		    if (j == varIndices.size()) {
			unsigned int* idxs = new unsigned int[2];
			idxs[0] = vindex;	// input index
			idxs[1] = rv;	// output index
                        // cerr << "adding varIndices, vindex=" << vindex << " rv=" << rv << endl;
			// if crossTerms, then all variables must
			// be in one input sample.
			if (_crossTerms) {
                            // cerr << "varIndices.size()=" << varIndices.size() << ", rv=" << rv << endl;
                            assert(varIndices.size() == rv);
                        }
			varIndices.push_back(idxs);
		    }

		    VLOG(("") << "StatisticsCruncher::attach, reqVariables[" <<
                  rv << "]=" << reqvar.getName() << 
                  " (" << reqvar.getUnits() <<
                  "), " << reqvar.getLongName() << 
                  " station=" << reqvar.getStation());
		}
	    }
	}
	if (varIndices.size() > 0)
	{
	    static LogContext lp(LOG_VERBOSE);
	    if (lp.active())
	    {
	        LogMessage msg(&lp);
		msg << "id=" << GET_DSM_ID(id) << ',' << hex << GET_SPS_ID(id)
	            <<  dec << " varIndices.size()=" << varIndices.size()
		    << " _reqVariables.size()=" << _reqVariables.size();
	        msg.log();
		msg << " varIndices:";
		for (unsigned int i = 0; i < varIndices.size(); ++i)
		    msg << " " << *varIndices[i];
		msg.log();
		msg << " reqVariables:";
		for (unsigned int i = 0; i < _reqVariables.size(); ++i)
		    msg << " " << _reqVariables[i]->getName();
		msg.log();
	    }
            _sampleMap[id] = sinfo;
	    // Should have one input sample if cross terms
	    if (_crossTerms) {
	        assert(_sampleMap.size() == 1);
	        assert(varIndices.size() == _reqVariables.size());
	    }
            VLOG(("") << "addSampleClientForTag, intag=" << intag->getDSMId() << ',' << intag->getSpSId() << '(' << hex << intag->getSpSId() << dec << ')');
            source->addSampleClientForTag(this,intag);
	}
    }
    if (nmatches < _ninvars) {
        ostringstream ost;
        for (unsigned int i = 0; i < _ninvars; i++) {
            if (!varMatches[i]) {
                if (ost.str().length() > 0) ost << ", ";
                ost << _reqVariables[i]->getName() + ":" <<
                    _reqVariables[i]->getSite()->getName() << '(' <<
                    _reqVariables[i]->getStation() << ')';
            }
        }
        WLOG(("StatisticsCruncher: no match for variables: ") << ost.str());
        // throw n_u::InvalidParameterException(
          //   "StatisticsCruncher","no match for variables",tmp);
    }
        
    if (_station >= 0) _outSample.setStation(_station);
    {
        static LogContext lp(LOG_VERBOSE);
        if (lp.active()) {
            LogMessage ost(&lp, "StatisticsCruncher: ");
            for (unsigned int i = 0; i < _reqVariables.size(); i++) {
                if (i > 0) ost << ", ";
                ost << _reqVariables[i]->getName() + "(" << _reqVariables[i]->getStation() << ")";
            }
            ost << ", _station=" << _station;
        }
    }
}

void StatisticsCruncher::splitNames()
{
    /* ISFS variables names use dots to separate portions of the name:
     * "w.5m.towerA", "h2o.5m.towerA"'.
     *
     * We want to create reasonably short names for covariances
     * and higher moments of these variables.
     * The second moment of "w" and "h2o" above is named
     * "w'h2o'.5m.towerA".
     *
     * When there are two sensors sampling the same variable
     * at a height, they are differentiated by a sensor field
     * after the variable name:
     * "h2o.licor.5m.towerA", "h2o.csi.5m.towerA".
     *
     * The second moments of these with "w" would be called:
     *      w'h2o'.(,licor).5m.towerA
     *      w'h2o'.(,csi).5m.towerA
     * To avoid the above syntax in the case when just one name has
     * the distinct middle section, the above becomes
     *      w'h2o'.licor.5m.towerA
     *      w'h2o'.csi.5m.towerA
     *
     * The parentheses syntax is used if there are two distinct
     * middle portions, which would be the case if there were two
     * measurements of "w", say "csat" and "ati" along with h2o.
     * The second moments would be called:
     *      w'h2o'.(csat,licor).5m.towerA
     *      w'h2o'.(csat,csi).5m.towerA
     *      w'h2o'.(ati,licor).5m.towerA,
     *      w'h2o'.(ati,csi).5m.towerA,
     *
     * Another scenario is when the trailing portions are not
     * equal, for example, from SCP:
     *   w.1m
     *   h2o.1m.M
     * where w.1m is a "station" variable, common to multiple stations,
     * and h2o.1m.M is unique to tower M.
     * The covariance should be named w'h2o'.1m.M, rather than the 
     * complicated w'h2o'.1m.(,M)
     *
     * This code does not handle the situation of covariances involving
     * variables with names like:
     *    w.ati.1m, w.csat.1m
     *    h2o.licor.1m.M, h2o.csi.1m.M
     * where two separate differences are found after the initial 
     * ariable name.
     * 
     * This function splits the variable names at the dots, then looks
     * for common leading and trailing portions of the names in order
     * to build the compressed names of higher moments.
     *
     */
    _splitVarNames.clear();
    for (unsigned int i = 0; i < _reqVariables.size(); i++) {
        const string& n = _reqVariables[i]->getName();
	vector<string> words;
	for (string::size_type cpos = 0;;) {
	    string::size_type dot = n.find('.',cpos+1);
	    if (dot == string::npos) {
	        words.push_back(n.substr(cpos));
		break;
	    }
	    words.push_back(n.substr(cpos,dot-cpos));
	    cpos = dot;
	}
	_splitVarNames.push_back(words);
    }

    // after the first word, look for strings in common
    unsigned int nw0 = _splitVarNames[0].size();
    unsigned int idx;
    for (idx = 1; idx < nw0; idx++) {
	const string& scom = _splitVarNames[0][idx];
	unsigned int i; 
	for (i = 1; i < _splitVarNames.size(); i++) {
	    unsigned int nw = _splitVarNames[i].size();
	    if (idx >= nw) break;
	    if (_splitVarNames[i][idx] != scom) break;
	}
	if (i < _splitVarNames.size()) break;
    }

    _leadCommon.clear();
    for (unsigned int i = 1; i < idx; i++)
        _leadCommon += _splitVarNames[0][i];

    /* remove common parts from split names */
    for (unsigned int i = 0; i < idx - 1; i++) {
        for (unsigned j = 0; j < _splitVarNames.size(); j++) {
            _splitVarNames[j].erase(_splitVarNames[j].begin() + 1);
        }
    }

    // compute how many trailing words the names have in common
    nw0 = _splitVarNames[0].size();
    unsigned int ncend;
    for (ncend = 0; ncend < nw0 - 1; ncend++) {
	const string& suff = _splitVarNames[0][nw0 - ncend - 1];
	unsigned int i; 
	for (i = 1; i < _splitVarNames.size(); i++) {
	    unsigned int nw = _splitVarNames[i].size();
	    if (nw - ncend - 1 < 1) break;
	    if (_splitVarNames[i][nw - ncend - 1] != suff) break;
	}
	if (i < _splitVarNames.size()) break;
    }
    _commonSuffix.clear();
    for (unsigned int i = nw0 - ncend; i <  nw0; i++)
        _commonSuffix += _splitVarNames[0][i];

    /* remove common suffix from split names */
    for (unsigned int i = 0; i < ncend; i++) {
        for (unsigned j = 0; j < _splitVarNames.size(); j++) {
            _splitVarNames[j].erase(_splitVarNames[j].end() - 1);
        }
    }
}

string StatisticsCruncher::makeName(int i, int j, int k, int l)
{
    unsigned int n;

    string name = _splitVarNames[i][0];
    if (j >= 0) {
	name += '\'';
	name += _splitVarNames[j][0];
	name += '\'';
	if (k >= 0) {
	    name += _splitVarNames[k][0];
	    name += '\'';
	    if (l >= 0) {
		name += _splitVarNames[l][0];
		name += '\'';
	    }
	}
    }

    // common words after the initial word
    name += _leadCommon;

    // middle section of name
    //
    vector<string> middles;
    string middle;
    for (n = 1; n < _splitVarNames[i].size(); n++) {
	if (n == 1) middle += _splitVarNames[i][n].substr(1);
	else middle += _splitVarNames[i][n];
    }
    middles.push_back(middle);
    if (j >= 0) {
	middle.clear();
	for (n = 1; n < _splitVarNames[j].size(); n++) {
	    if (n == 1) middle += _splitVarNames[j][n].substr(1);
	    else middle += _splitVarNames[j][n];
	}
	middles.push_back(middle);
	if (k >= 0) {
	    middle.clear();
	    for (n = 1; n < _splitVarNames[k].size(); n++) {
		if (n == 1) middle += _splitVarNames[k][n].substr(1);
		else middle += _splitVarNames[k][n];
	    }
	    middles.push_back(middle);
	    if (l >= 0) {
                middle.clear();
		for (n = 1; n < _splitVarNames[l].size(); n++) {
		    if (n == 1) middle += _splitVarNames[l][n].substr(1);
		    else middle += _splitVarNames[l][n];
		}
                middles.push_back(middle);
	    }
	}
    }
    for (n = 1; n < middles.size(); n++)
    	if (middles[0] != middles[n]) break;

    if (n == middles.size()) {	// all the same
	if (middles[0].length() > 0)
	    name += string(".") + middles[0];
    }
    else {
        // If only one variable name has a "middle" use it.
        int nmiddle = 0;
        string mid;
        for (n = 0; n < middles.size(); n++) {
            if (middles[n].length() > 0) {
                mid = middles[n];
                nmiddle++;
            }
        }
        if (nmiddle == 1) name += string(".") + mid;
        else {
            // otherwize use the kludge ".(mid1,mid2)" syntax
            // which is supposed to indicate that the 2nd moment
            // is made of variables from differing sites, or sensors.
            name += string(".(");
            for (n = 0; n < middles.size(); n++) {
                if (n > 0) name += string(",");
                name += middles[n];
            }
            name += string(")");
        }
    }

    // suffix
    name += _commonSuffix;
    return name;
}

string StatisticsCruncher::makeUnits(int i, int j, int k, int l)
{
    vector<string> unitsVec;
    if (_reqVariables[i]->getConverter())
    	unitsVec.push_back(_reqVariables[i]->getConverter()->getUnits());
    else unitsVec.push_back(_reqVariables[i]->getUnits());

    if (j >= 0) {
	if (_reqVariables[j]->getConverter())
	    unitsVec.push_back(_reqVariables[j]->getConverter()->getUnits());
	else unitsVec.push_back(_reqVariables[j]->getUnits());
	if (k >= 0) {
	    if (_reqVariables[k]->getConverter())
		unitsVec.push_back(_reqVariables[k]->getConverter()->getUnits());
	    else unitsVec.push_back(_reqVariables[k]->getUnits());
	    if (l >= 0)
	    {
		if (_reqVariables[l]->getConverter())
		    unitsVec.push_back(_reqVariables[l]->getConverter()->getUnits());
		else unitsVec.push_back(_reqVariables[l]->getUnits());
	    }
	}
    }
    return makeUnits(unitsVec);
}

std::string StatisticsCruncher::makeUnits(const std::vector<std::string>& units)
{
    string res;
    vector<bool> used(units.size(),false);

    unsigned int i,j;
    for (i = 0; i < units.size(); i++) {
	if (used[i]) continue;
	int ndup = 1;
	for (j = i + 1; j < units.size(); j++)
	    if (units[i] == units[j]) {
	        ndup++;
		used[j] = true;
	    }
	if (res.length() > 0) res += string(" ");
	if (ndup == 1) res += units[i];
	else {
	    ostringstream ost;
	    ost << '(' << units[i] << ')' << '^' << ndup;
	    res += ost.str();
	}
    }
    return res;
}

void StatisticsCruncher::setupMoments(unsigned int nv,unsigned int nmoment)
{
    for (unsigned int i = 0; i < nv; i++) {
	string name;
	string units;
        string longname;
	switch (nmoment) {
	case 1:
	    name= makeName(i);
	    units = makeUnits(i);
            // on first moments set longname from variable's longname
            longname = _reqVariables[i]->getLongName();
	    _nsum = nv;
	    _n1mom = nv;
	    break;
	case 2:
	    name= makeName(i,i);
	    units = makeUnits(i,i);
            longname = "2nd moment";
	    _n2mom = nv;
	    break;
	case 3:
	    name= makeName(i,i,i);
	    units = makeUnits(i,i,i);
            longname = "3rd moment";
	    _n3mom = nv;
	    break;
	case 4:
	    name= makeName(i,i,i,i);
	    units = makeUnits(i,i,i,i);
            longname = "4th moment";
	    _n4mom = nv;
	    break;
	}
        addVariable(name, longname, units);
    }
}


void
StatisticsCruncher::
addVariable(const std::string& name,
            const std::string& longname,
            const std::string& units)
{
    if (_outSample.getVariables().size() <= _nOutVar) {
        Variable* v = new Variable();
        _outSample.addVariable(v);
    }
    Variable& var = _outSample.getVariable(_nOutVar);
    var.setName(name);
    var.setLongName(longname);
    var.setUnits(units);
    _nOutVar++;
}


void StatisticsCruncher::setupWindDir()
{
    /*
     * From u,U and v,V, create dir,Dir
     */

    if (_splitVarNames.size() != 2 || _splitVarNames[0].empty() || _splitVarNames[1].empty()) throw n_u::InvalidParameterException(
    	"StatisticsCruncher","winddir","input variables must be u or U and v or V");
    string uname = _splitVarNames[0][0];
    if (uname != "u" && uname != "U") throw n_u::InvalidParameterException(
    	"StatisticsCruncher","winddir","first input variable must be u or U");

    string vname = _splitVarNames[1][0];
    if (vname != "v" && vname != "V" ) throw n_u::InvalidParameterException(
    	"StatisticsCruncher","winddir","second input variable must be v or V");

    string dir = "Dir";
    if (uname == "u") dir = "dir";
    dir += _leadCommon;
    dir += _commonSuffix;

    if (_nOutVar < 1) {
        Variable* nv = new Variable();
        nv->setName(dir);
        nv->setLongName("Vector wind direction");
        nv->setUnits("deg");
        _outSample.addVariable(nv);
        _nOutVar++;
    }
    _nsum = 2;  // number of sums to maintain
    _n1mom = 1; // number of 1st moments out: dir
}

void StatisticsCruncher::setupCovariances()
{
    _nsum = _ninvars;
    _ncov = (_ninvars * (_ninvars + 1)) / 2;

    for (unsigned int i = 0; i < _ninvars; i++) {
	for (unsigned int j = i; j < _ninvars; j++) {
	    string name = makeName(i,j);
	    string units = makeUnits(i,j);

	    if (_outSample.getVariables().size() <= _nOutVar) {
		Variable* v = new Variable();
		_outSample.addVariable(v);
	    }
            Variable& var = _outSample.getVariable(_nOutVar);
            var.setName(name);
            var.setLongName("2nd moment");
            var.setUnits(units);
            _nOutVar++;
	}
    }
}

void StatisticsCruncher::setupTrivariances()
{
    _nsum = _ninvars;
    _ntri = 0;
    for (unsigned int i = 0; i < _ninvars; i++) {
	for (unsigned int j = i; j < _ninvars; j++) {
	    for (unsigned int k = j; k < _ninvars; k++,_ntri++) {
		string name = makeName(i,j,k);
		string units = makeUnits(i,j,k);

		if (_outSample.getVariables().size() <= _nOutVar) {
		    Variable* v = new Variable();
		    _outSample.addVariable(v);
		}
                Variable& var = _outSample.getVariable(_nOutVar);
                var.setName(name);
                var.setLongName("3rd moment");
                var.setUnits(units);
                _nOutVar++;
	    }
	}
    }
}

/* A pruned trivariance.
 *  u,v,w are wind components
 *  s       is a scalar
 *  x       is any of the above
 *
 *  N       number of variables
 *  N-3	  number of scalars
 *
 * These combinations are computed:
 *		# combinations
 *  x^3		N	3rd moment
 *  wws	        N-3
 *  wss		N-3	w vs scalar-scalar (no scalar cross terms)
 *  [uv][uvw]w	5	(uvw, vuw are duplicates)
 *  [uv]ws	2 * (N - 3)
 *
 *  total:	5 * N - 7
 */
void StatisticsCruncher::setupPrunedTrivariances()
{

    unsigned int i,j;

    _nsum = _ninvars;
    _ntri = 5 * _ninvars - 7;

    /*
     * Warning, the indices in triComb must be increasing.
     *
     * In a misguided attempt to save space, xySum is a
     * triangular matrix, and attempts to access below the
     * diagonal, like xySum[i][j], j < i, will result in gibberish.
     * As a result, this must be true:
     *	triComb[n][0] <= triComb[n][1] <= triComb[n][2]
     * This is checked for in the assert()s, below.
     */
    _triComb = new unsigned int*[_ntri];
    for (i=0; i < _ntri; i++) _triComb[i] = new unsigned int[3];

    unsigned int nt = 0;

    /* x^3 3rd moments */
    for (i = 0; i < _ninvars; i++,nt++)
	_triComb[nt][0] = _triComb[nt][1] = _triComb[nt][2] = i;
    setupMoments(_ninvars,3);
    _n3mom = 0;	// accounted for in ntri

    // wws trivariances
    i = 2;
    for (j = 3; j < _ninvars; j++,nt++) {
	_triComb[nt][0] = i;
	_triComb[nt][1] = i;
	_triComb[nt][2] = j;

	string name = makeName(i,i,j);
	string units = makeUnits(i,i,j);

	if (_outSample.getVariables().size() <= _nOutVar) {
	    Variable* v = new Variable();
	    _outSample.addVariable(v);
	}
        Variable& var = _outSample.getVariable(_nOutVar);
        var.setName(name);
        var.setLongName("3rd moment");
        var.setUnits(units);
        _nOutVar++;
    }
    // ws^2 trivariances
    i = 2;
    for (j = 3; j < _ninvars; j++,nt++) {
	_triComb[nt][0] = i;
	_triComb[nt][1] = j;
	_triComb[nt][2] = j;

	string name = makeName(i,j,j);
	string units = makeUnits(i,j,j);

	if (_outSample.getVariables().size() <= _nOutVar) {
	    Variable* v = new Variable();
	    _outSample.addVariable(v);
	}
        Variable& var = _outSample.getVariable(_nOutVar);
        var.setName(name);
        var.setLongName("3rd moment");
        var.setUnits(units);
        _nOutVar++;
    }
    // [uv][uvw]w trivariances
    for (i = 0; i < 2; i++) {
	for (j = i; j < 3; j++,nt++) {

	    _triComb[nt][0] = i;
	    _triComb[nt][1] = j;
	    _triComb[nt][2] = 2;

	    string name = makeName(i,j,2);
	    string units = makeUnits(i,j,2);

	    if (_outSample.getVariables().size() <= _nOutVar) {
		Variable* v = new Variable();
		_outSample.addVariable(v);
	    }
            Variable& var = _outSample.getVariable(_nOutVar);
            var.setName(name);
            var.setLongName("3rd moment");
            var.setUnits(units);
            _nOutVar++;
	}
    }
    // uws, vws trivariances
    for (i = 0; i < 2; i++) {
	for (unsigned int k = 3; k < _ninvars; k++,nt++) {

	    _triComb[nt][0] = i;
	    _triComb[nt][1] = 2;
	    _triComb[nt][2] = k;

	    string name = makeName(i,2,k);
	    string units = makeUnits(i,2,k);

	    if (_outSample.getVariables().size() <= _nOutVar) {
		Variable* v = new Variable();
		_outSample.addVariable(v);
	    }
            Variable& var = _outSample.getVariable(_nOutVar);
            var.setName(name);
            var.setLongName("3rd moment");
            var.setUnits(units);
            _nOutVar++;
	}
    }

#ifdef DEBUG
    cerr << "nt=" << nt << " ntri=" << _ntri << endl;
#endif
    assert(nt==_ntri);
    for (nt=0; nt < _ntri ; nt++)
	assert(_triComb[nt][0] <= _triComb[nt][1] &&
	      _triComb[nt][1] <= _triComb[nt][2]);

}

/*
 * covariances, including scalar variances,
 * but no scalar-scalar cross-covariances.
 */
void StatisticsCruncher::setupFluxes()
{
    _nsum = _ninvars;
    _ncov = 4 * _ninvars - 6;
    unsigned int nc = 0;
    for (unsigned int i = 0; i < _ninvars; i++) {
	for (unsigned int j = i; j < (i > 2 ? i + 1 : _ninvars); j++,nc++) {
	    string name = makeName(i,j);
	    string units = makeUnits(i,j);

	    if (_outSample.getVariables().size() <= _nOutVar) {
		Variable* v = new Variable();
		_outSample.addVariable(v);
	    }
            Variable& var = _outSample.getVariable(_nOutVar);
            var.setName(name);
            var.setLongName("2nd moment");
            var.setUnits(units);
            _nOutVar++;
	}
    }
    assert(nc==_ncov);
}

/*
 * covariances, including scalar variances,
 * but no scalar-scalar cross-covariances or variances.
 * This is typically used when computing fluxes of
 * a scalar against winds from a second sonic.
 */
void StatisticsCruncher::setupReducedFluxes()
{
    _nsum = _ninvars;
    _ncov = 3 * _ninvars - 3;	// no scalar:scalar terms
    unsigned int nc = 0;
    for (unsigned int i = 0; i < _ninvars && i < 3; i++) {
	for (unsigned int j = i; j < _ninvars; j++,nc++) {
	    string name = makeName(i,j);
	    string units = makeUnits(i,j);

	    if (_outSample.getVariables().size() <= _nOutVar) {
		Variable* v = new Variable();
		_outSample.addVariable(v);
	    }
            Variable& var = _outSample.getVariable(_nOutVar);
            var.setName(name);
            var.setLongName("2nd moment");
            var.setUnits(units);
            _nOutVar++;
	}
    }
    assert(nc==_ncov);
}

/*
 * covariances combinations of first component with itself
 * and others:  scalar,u,v,w.
 * This is typically used when the sampling rate of a scalar
 * is much different (often lower) from others, and you want to avoid
 * loss of other data when resampling the slow variable with others.
 */
void StatisticsCruncher::setupReducedScalarFluxes()
{
    _nsum = _ninvars;
    _ncov = _ninvars;		// covariance of first scalar 
    				// against all others
    for (unsigned int j = 0; j < _ninvars; j++) {
	string name = makeName(j,0);	// flip names
	string units = makeUnits(j,0);

	if (_outSample.getVariables().size() <= _nOutVar) {
	    Variable* v = new Variable();
	    _outSample.addVariable(v);
	}
        Variable& var = _outSample.getVariable(_nOutVar);
        var.setName(name);
        var.setLongName("2nd moment");
        var.setUnits(units);
        _nOutVar++;
    }
}

void StatisticsCruncher::setupMinMax(const string& suffix)
{
    _nsum = 0;
    _n1mom = _ninvars;

    for (unsigned int i = 0; i < _ninvars; i++) {
	// add the suffix to the first word
	string name = _splitVarNames[i][0] + suffix;
        name += _leadCommon;
	for (unsigned int n = 1; n < _splitVarNames[i].size(); n++)
	    name += _splitVarNames[i][n];
        name += _commonSuffix;

	if (_outSample.getVariables().size() <= _nOutVar) {
	    Variable* v = new Variable();
	    _outSample.addVariable(v);
	}
        Variable& var = _outSample.getVariable(_nOutVar);
        var.setName(name);
        var.setUnits(makeUnits(i));
        _nOutVar++;
    }
}

void StatisticsCruncher::createCombinations()
{
    if (_triComb) {
	for (unsigned int i=0; i < _ntri; i++) delete [] _triComb[i];
	delete [] _triComb;
	_triComb = 0;
    }

    splitNames();
    _nOutVar = 0;
    _ncov = _ntri = _n1mom = _n2mom = _n3mom = _n4mom = 0;

    switch(_statsType) {
    case STATS_SUM:
        // Assume that the variable is a measure of amount over a range of
        // time, ie, rain accumulation, so that it makes sense to sum it
        // over the stats period, and the resulting measurement has the
        // same name and units.  Fall through and setup same as for MEAN,
        // but the result will not be divided by N.
    case STATS_MEAN:
	setupMoments(_ninvars,1);
	break;
    case STATS_VAR:
	setupMoments(_ninvars,1);
	setupMoments(_ninvars,2);
	break;
    case STATS_MINIMUM:
	setupMinMax("_min");
	break;
    case STATS_MAXIMUM:
	setupMinMax("_max");
	break;
    case STATS_COV:
	setupMoments(_ninvars,1);
	setupCovariances();
        if (_higherMoments) {
            setupMoments(_ninvars,3);
            setupMoments(_ninvars,4);
        }
	break;
    case STATS_TRIVAR:
	setupMoments(_ninvars,1);
	setupCovariances();
	setupTrivariances();
        if (_higherMoments)
            setupMoments(_ninvars,4);
	break;
    case STATS_PRUNEDTRIVAR:
	setupMoments(_ninvars,1);
	setupCovariances();
	setupPrunedTrivariances();
        if (_higherMoments)
            setupMoments(_ninvars,4);
	break;
    case STATS_FLUX:
	setupMoments(_ninvars,1);
	setupFluxes();
        if (_higherMoments) {
            setupMoments(_ninvars,3);
            setupMoments(_ninvars,4);
        }
	break;
    case STATS_RFLUX:
	setupMoments(3,1);	// means of winds only
	setupReducedFluxes();
        if (_higherMoments) {
            setupMoments(3,3);
            setupMoments(3,4);
        }
	break;
    case STATS_SFLUX:
	setupMoments(1,1);	// means of scalar only
	setupReducedScalarFluxes();	// covariances of first member
        if (_higherMoments) {
            setupMoments(1,3);
            setupMoments(1,4);
        }
	break;
    case STATS_WINDDIR:
	setupWindDir();
        break;
    default:
	break;
    }
    _ntot = _n1mom + _n2mom + _ncov + _ntri + _n3mom + _n4mom;
    VLOG(("") << "ntot=" << _ntot << " outsamp vars="
         << _outSample.getVariables().size() << "\n"
         << "n1mom=" << _n1mom << " n2mom=" << _n2mom
         << " ncov=" << _ncov << " ntri=" << _ntri
         << " n3mom=" << _n3mom << " n4mom=" << _n4mom);
    assert(_ntot == _outSample.getVariables().size());
    assert(_nOutVar == _ntot);

    static LogContext lp(LOG_VERBOSE);
    if (lp.active())
    {
        LogMessage msg;
        msg << "createCombinations: ";
        VariableIterator vi = _outSample.getVariableIterator();
        for ( ; vi.hasNext(); ) {
            const Variable* var = vi.next();
            cerr << var->getName() << '(' << var->getStation() << ") ";
        }
    }
    for (unsigned int i = 0; i < _outSample.getVariables().size(); i++)
    {
        _outSample.getVariable(i).setStation(_station);
    }
}

void StatisticsCruncher::initStats()
{
    /* If _numpoints are to be output, and an additional variable
     * has not been defined for it, add a "counts" output variable
     * to the sample.
     */
    if (_numpoints && _ntot == _outSample.getVariables().size()) {
	Variable* v = new Variable();
	v->setType(Variable::WEIGHT);
	v->setUnits("");
        if (_station >= 0) v->setStation(_station);

	if (_countsName.length() == 0) {
	    _countsName = "counts";
	    // add a suffix to the counts name
            _countsName += _leadCommon;
            _countsName += _commonSuffix;
            v->setName(_countsName);
            v->setStation(_station);
            _countsName = v->getName();
	}

        /* try to make it unique within the StatisticsProcessor */
        _countsName = _proc->getUniqueCountsName(_countsName);
	v->setName(_countsName);
	_outSample.addVariable(v);
    }

    _outlen = _outSample.getVariables().size();

    unsigned int i,n;

    if (_statsType == STATS_MINIMUM) {
	delete [] _xMin;
	_xMin = new float[_ninvars];
    }
    else if (_statsType == STATS_MAXIMUM) {
	delete [] _xMax;
	_xMax = new float[_ninvars];
    }
    else {
	delete [] _xSum;
	_xSum = new double[_nsum];
    }

    _nSamples = new unsigned int[_nsum];

    /*
     * Create array of pointers so that xySum[i][j], for j >= i,
     * points to the right element in the sparse array.
     */
    if (_xySum) delete [] _xySum[0];
    delete [] _xySum;
    _xySum = 0;
    if (_ncov > 0) {
	double *xySumArray = new double[_ncov];

	n = (_statsType == STATS_SFLUX ? 1 : _ninvars);
	_xySum = new double*[n];

	for (i = 0; i < n; i++) {
	    _xySum[i] = xySumArray - i;
	    if ((_statsType == STATS_FLUX || _statsType == STATS_RFLUX) && i > 2)
	    	xySumArray++;
	    else xySumArray += _ninvars - i;
	}
    }
    else if (_n2mom > 0) {
	assert(_n2mom == _ninvars);
	double *xySumArray = new double[_n2mom];
	_xySum = new double*[_n2mom];
	for (i = 0; i < _ninvars; i++)
	    _xySum[i] = xySumArray;
    }

    delete [] _xyzSum;
    _xyzSum = 0;
    if (_ntri > 0) _xyzSum = new double[_ntri];
    else if (_n3mom > 0) _xyzSum = new double[_n3mom];

    delete [] _x4Sum;
    _x4Sum = 0;
    if (_n4mom > 0) _x4Sum = new double[_n4mom];
}

void StatisticsCruncher::zeroStats()
{
    unsigned int i;
    if (_xSum) for (i = 0; i < _nsum; i++) _xSum[i] = 0.;
    for (i = 0; i < _ncov; i++) _xySum[0][i] = 0.;
    for (i = 0; i < _n2mom; i++) _xySum[0][i] = 0.;
    for (i = 0; i < _ntri; i++) _xyzSum[i] = 0.;
    for (i = 0; i < _n3mom; i++) _xyzSum[i] = 0.;
    for (i = 0; i < _n4mom; i++) _x4Sum[i] = 0.;
    if (_xMin) for (i = 0; i < _ninvars; i++) _xMin[i] = 1.e37;
    if (_xMax) for (i = 0; i < _ninvars; i++) _xMax[i] = -1.e37;
    for (i = 0; i < _nsum; i++) _nSamples[i] = 0;
}

bool StatisticsCruncher::receive(const Sample* samp) throw()
{
    assert(samp->getType() == FLOAT_ST || samp->getType() == DOUBLE_ST);

    dsm_sample_id_t id = samp->getId();

    map<dsm_sample_id_t,sampleInfo >::iterator vmi =
    	_sampleMap.find(id);
    if (vmi == _sampleMap.end()) {
        WLOG(("unrecognized sample, id=") << samp->getDSMId() << ',' << samp->getSpSId() <<
            " sampleMap.size()=" << _sampleMap.size());
        return false;	// unrecognized sample
    }

    dsm_time_t tt = samp->getTimeTag();
    while (tt > _tout) {
        if (tt > _endTime.toUsecs()) return false;
	if (_tout != LONG_LONG_MIN) {
	    computeStats();
	}
	if (!getFillGaps() || _tout == LONG_LONG_MIN) 
            _tout = tt - (tt % _periodUsecs) + _periodUsecs;
        else
            _tout += _periodUsecs;
    }
    if (tt < _tout - _periodUsecs) return false;

    struct sampleInfo& sinfo = vmi->second;
    const vector<unsigned int*>& vindices = sinfo.varIndices;

    unsigned int nvarsin = vindices.size();
    unsigned int nvsamp = samp->getDataLength();

    unsigned int i,j,k;
    unsigned int vi,vj,vk,vo;
    double *xySump,*xyzSump;
    double x;
    double xy;

    unsigned int nonNANs = 0;
    if (sinfo.weightsIndex < nvsamp)
    	nonNANs = (unsigned int)samp->getDataValue(sinfo.weightsIndex);
    else if (_crossTerms) {
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    if(vi < nvsamp && !std::isnan(samp->getDataValue(vi))) nonNANs++;
	}
    }

    static LogContext lp(LOG_VERBOSE);
    if (lp.active())
    {
        LogMessage msg;
        n_u::UTime ut(tt);
        msg << ut.format(true,"%Y %m %d %H:%M:%S.%6f")
            << " id=" << samp->getDSMId() << ','
            << samp->getSpSId() << ' ';
        for (unsigned int i = 0; i < nvsamp; i++)
            msg << samp->getDataValue(i) << ' ';
    }

    if (_crossTerms && nonNANs < _ninvars) return false;

    switch (_statsType) {
    case STATS_MINIMUM:
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    if (vi < nvsamp && !std::isnan(x = samp->getDataValue(vi))) {
		vo = vindices[i][1];
		if (x < _xMin[vo]) _xMin[vo] = x;
		_nSamples[vo]++;
	    }
	}
	return true;
    case STATS_MAXIMUM:
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    if (vi < nvsamp && !std::isnan(x = samp->getDataValue(vi))) {
		vo = vindices[i][1];
		if (x > _xMax[vo]) _xMax[vo] = x;
		_nSamples[vo]++;
	    }
	}
	return true;
    case STATS_MEAN:
    case STATS_SUM:
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    if (vi < nvsamp && !std::isnan(x = samp->getDataValue(vi))) {
		vo = vindices[i][1];
		_xSum[vo] += x;
		_nSamples[vo]++;
	    }
	}
	return true;
    case STATS_WINDDIR:
        {
            vi = vindices[0][0];
            double u = samp->getDataValue(vi);
            vi = vindices[1][0];
            double v = samp->getDataValue(vi);
            if (!std::isnan(u) && !std::isnan(v)) {
                _xSum[0] += u;
                _xSum[1] += v;
		_nSamples[0]++;
            }
	}
	return true;
    case STATS_VAR:
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    if (vi < nvsamp && !std::isnan(x = samp->getDataValue(vi))) {
		vo = vindices[i][1];
		_xSum[vo] += x;
		_xySum[vo][vo] += x * x;
		_nSamples[vo]++;
	    }
	}
	return true;
    case STATS_COV:
	// cross term product, all input data is present and non-NAN
	xySump = _xySum[0];
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
            assert(vi < nvsamp);
	    // crossterms, so: vindices[i][1] == i;
	    x = samp->getDataValue(vi);
	    _xSum[i] += x;
	    for (j = i; j < nvarsin; j++) {
		vj = vindices[j][0];
                assert(vj < nvsamp);
		xy = x * samp->getDataValue(vj);
		*xySump++ += xy;
	    }
            if (_higherMoments) {
                _xyzSum[i] += (xy = x * x * x);
                _x4Sum[i] += xy * x;
            }
	}
	_nSamples[0]++;		// only need one nSamples
	break;
    case STATS_FLUX:
	// no scalar:scalar cross terms
	// cross term product, all input data is present and non-NAN
	xySump = _xySum[0];
	for (i = 0; i < 3; i++) {
	    vi = vindices[i][0];
            assert(vi < nvsamp);
	    // crossterms, so: vindices[i][1] == i;
	    x = samp->getDataValue(vi);
	    _xSum[i] += x;
	    for (j = i; j < nvarsin; j++) {
		vj = vindices[j][0];
                assert(vj < nvsamp);
		xy = x * samp->getDataValue(vj);
		*xySump++ += xy;
	    }
            if (_higherMoments) {
                _xyzSum[i] += (xy = x * x * x);
                _x4Sum[i] += xy * x;
            }
	}
	for (; i < nvarsin; i++) {	// scalar means and variances
	    vi = vindices[i][0];
            assert(vi < nvsamp);
	    x = samp->getDataValue(vi);
	    _xSum[i] += x;
	    *xySump++ += (xy = x * x);
            if (_higherMoments) {
                _xyzSum[i] += (xy *= x);
                _x4Sum[i] += xy * x;
            }
	}
	_nSamples[0]++;		// only need one nSamples
	break;
    case STATS_RFLUX:	
	// only wind:scalar cross terms, no scalar:scalar terms
	// cross term product, all input data is present and non-NAN
	xySump = _xySum[0];	
	for (i = 0; i < 3; i++) {
	    vi = vindices[i][0];
            assert(vi < nvsamp);
	    // crossterms, so: vindices[i][1] == i;
	    x = samp->getDataValue(vi);
	    _xSum[i] += x;
	    for (j = i; j < nvarsin; j++) {
		vj = vindices[j][0];
                assert(vj < nvsamp);
		xy = x * samp->getDataValue(vj);
		*xySump++ += xy;
	    }
            if (_higherMoments) {
                _xyzSum[i] += (xy = x * x * x);
                _x4Sum[i] += xy * x;
            }
	}
	for (; i < nvarsin; i++) {	// scalar means
	    vi = vindices[i][0];
            assert(vi < nvsamp);
	    _xSum[i] += samp->getDataValue(vi);
	}
	_nSamples[0]++;		// only need one nSamples
	break;
    case STATS_SFLUX:	
	// first term is scaler
	// cross term product, all input data is present and non-NAN
	xySump = _xySum[0];		// no wind:wind terms
	i = 0;
	vi = vindices[i][0];
        assert(vi < nvsamp);
	// crossterms, so: vindices[i][1] == i;
	x = samp->getDataValue(vi);
	for (j = i; j < nvarsin; j++) {
	    vj = vindices[j][0];
            assert(vj < nvsamp);
	    _xSum[j] += samp->getDataValue(vj);
	    xy = x * samp->getDataValue(vj);
	    *xySump++ += xy;
	}
        if (_higherMoments) {
            _xyzSum[i] += (xy = x * x * x);
            _x4Sum[i] += xy * x;
        }
	_nSamples[0]++;		// only need one nSamples
	break;
    case STATS_TRIVAR:
	// cross term product, all input data is present and non-NAN
	xySump = _xySum[0];
	xyzSump = _xyzSum;
	for (i=0; i < nvarsin; i++) {	// no scalar:scalar cross terms
	    vi = vindices[i][0];
            assert(vi < nvsamp);
	    // crossterms, so: vindices[i][1] == i;
	    x = samp->getDataValue(vi);
	    _xSum[i] += x;
	    for (j = i; j < nvarsin; j++) {
		vj = vindices[j][0];
                assert(vj < nvsamp);
		xy = x * samp->getDataValue(vj);
		*xySump++ += xy;
		for (k=j; k < nvarsin; k++) {
		    vk = vindices[k][0];
                    assert(vk < nvsamp);
		    *xyzSump++ += xy * samp->getDataValue(vk);
		    if (_higherMoments && k == i) _x4Sum[i] += xy * x * x;
		}
	    }
	}
	_nSamples[0]++;		// only need one nSamples
	break;
    case STATS_PRUNEDTRIVAR:
	// cross term product, all input data is present and non-NAN
	xySump = _xySum[0];
	xyzSump = _xyzSum;
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
            assert(vi < nvsamp);
	    // crossterms, so: vindices[i][1] == i;
	    x = samp->getDataValue(vi);
	    _xSum[i] += x;
	    for (j = i; j < nvarsin; j++) {
		vj = vindices[j][0];
                assert(vj < nvsamp);
		xy = x * samp->getDataValue(vj);
		*xySump++ += xy;
	    }
	    if (_higherMoments) _x4Sum[i] += x * x * x * x;
	}
	for (unsigned int n = 0; n < _ntri; n++) {
	    i = _triComb[n][0];
	    j = _triComb[n][1];
	    k = _triComb[n][2];
	    vi = vindices[i][0];
            assert(vi < nvsamp);
	    vj = vindices[j][0];
            assert(vj < nvsamp);
	    vk = vindices[k][0];
            assert(vk < nvsamp);
	    *xyzSump++ += samp->getDataValue(vi) *
	    	samp->getDataValue(vj) * samp->getDataValue(vk);
	}
	_nSamples[0]++;		// only need one nSamples
	break;
    case STATS_UNKNOWN:
        break;
    }
    return true;
}

void StatisticsCruncher::computeStats()
{
    double *xyzSump;
    unsigned int i,j,k,l,n,nx,nr;
    double x,xm,xr;

    if (_tout == LONG_LONG_MIN) return;

    SampleT<float>* osamp = getSample<float>(_outlen);
    osamp->setTimeTag(_tout - _periodUsecs / 2);
    osamp->setId(_outSample.getId());
    // osamp->setId(0);
    float* outData = osamp->getDataPtr();
    int nSamp = _nSamples[0];

    l = 0;
    switch (_statsType) {
    case STATS_MINIMUM:
	for (i=0; i < _ninvars; i++) {
	    if (_nSamples[i] > 0) outData[l++] = _xMin[i];
	    else outData[l++] = floatNAN;
	}
	break;
    case STATS_MAXIMUM:
	for (i=0; i < _ninvars; i++) {
	    if (_nSamples[i] > 0) outData[l++] = _xMax[i];
	    else outData[l++] = floatNAN;
	}
	break;
    case STATS_MEAN:
	for (i=0; i < _ninvars; i++) {
	    if (_nSamples[i] > 0) outData[l++] = _xSum[i] / _nSamples[i];
	    else outData[l++] = floatNAN;
	}
	break;
    case STATS_SUM:
	for (i=0; i < _ninvars; i++) {
            // If there were no good samples in this period, then call the
            // sum a missing value.  Reporting a sum of 0 could imply the
            // sensor was working but reporting zero amounts.  So
            // distinguish between actual 0 amounts and no reports, and let
            // users decide how to interpret a sum with a missing value.
	    if (_nSamples[i] > 0)
                outData[l++] = _xSum[i];
	    else
                outData[l++] = floatNAN;
	}
	break;
    case STATS_WINDDIR:
        if (_nSamples[0] > 0) {
            double u = _xSum[0] / _nSamples[0];
            double v = _xSum[1] / _nSamples[0];
            outData[l++] = n_u::dirFromUV(u, v);
	}
        else {
            outData[l++] = floatNAN;
        }
	break;
    default:
	/* All other types have cross-terms and use only nSamples[0] counter */
	if (nSamp == 0) {
	    for (l=0; l < _ntot; l++) outData[l] = floatNAN;
	    break;
	}

	for (i=0; i < _ninvars; i++) {
	    _xSum[i] /= nSamp;  	// compute mean
	    if (i < _n1mom) outData[l++] = (float)_xSum[i];
	}

	// 2nd order
	nr = _ninvars;
	if (_statsType == STATS_RFLUX && _ninvars > 3) nr = 3;
	if (_statsType == STATS_SFLUX && _ninvars > 3) nr = 1;

	for (i = 0; i < nr; i++) {
	    // no cross terms in STATS_VAR or in STATS_FLUX for scalar:scalar terms
	    nx = (_statsType == STATS_VAR || (_statsType == STATS_FLUX && i > 2) ? i+1 : _ninvars);
	    for (j=i; j < nx; j++,l++) {
		xr = _xySum[i][j] / nSamp - _xSum[i] * _xSum[j];
		if ((i == j && xr < 0.0) || nSamp < 2) xr = 0.0;
		outData[l] = xr;
	    }
	}

	// 3rd order
	switch (_statsType) {
	case STATS_TRIVAR:
	    xyzSump = _xyzSum;
	    for (i = 0; i < _ninvars; i++) {
		xm = _xSum[i];
		for (j = i; j < _ninvars; j++)
		  for (k = j; k < _ninvars; k++,l++) {
		    xr = (((x = *xyzSump++)
			 - xm      * _xySum[j][k]
			 - _xSum[j] * _xySum[i][k]
			 - _xSum[k] * _xySum[i][j]) / nSamp)
			 + ( 2. * xm * _xSum[j] * _xSum[k]);
		    if (nSamp < 2) xr = 0.;
		    outData[l] = xr;
		    if (_higherMoments && k == i)
		      _x4Sum[i] = _x4Sum[i] / nSamp
				 - 4. * xm * x / nSamp
				 + 6. * xm * xm * _xySum[i][i] / nSamp
				 - 3. * xm * xm * xm * xm;
		  }
	    }
            if (_higherMoments) {
                for (i = 0; i < _ninvars; i++,l++) {
                    xr = _x4Sum[i];
                    if (xr < 0.) xr = 0.;
                    outData[l] = xr;
                }
            }
	    break;
	case STATS_PRUNEDTRIVAR:
	    xyzSump = _xyzSum;
	    for (n = 0; n < _ntri; n++,l++) {
		i = _triComb[n][0];
		j = _triComb[n][1];
		k = _triComb[n][2];
		// When referencing xySum[i][j], j must be >= i
		xm = _xSum[i];
		xr = (((x = *xyzSump++)
			- _xSum[i] * _xySum[j][k]
			- _xSum[j] * _xySum[i][k]
			- _xSum[k] * _xySum[i][j]) / nSamp)
			+ ( 2. * _xSum[i] * _xSum[j] * _xSum[k]);
		if (nSamp < 2) xr = 0.;
		outData[l] = xr;
		if (_higherMoments && i == j && j == k)
		  _x4Sum[i] = _x4Sum[i] / nSamp
			     - 4. * xm * x / nSamp
			     + 6. * xm * xm * _xySum[i][i] / nSamp
			     - 3. * xm * xm * xm * xm;
	    }
            if (_higherMoments) {
                for (i = 0; i < _ninvars; i++,l++) {
                    xr = _x4Sum[i];
                    if (xr < 0.) xr = 0.;
                    outData[l] = xr;
                }
            }
	    break;
	default:
	    // Third order moments
	    if (_n3mom > 0)
		for (i = 0; i < nr; i++,l++) {
		  xm = _xSum[i];
		  xr = _xyzSum[i] / nSamp
			- 3. * xm * _xySum[i][i] / nSamp
			+ 2. * xm * xm * xm;
		  if (nSamp < 2) xr = 0.;
		  outData[l] = xr;
		}

	    if (_n4mom > 0)
		for (i = 0; i < nr; i++,l++) {
		  xm = _xSum[i];
		  xr = _x4Sum[i] / nSamp
			- 4. * xm * _xyzSum[i] / nSamp
			+ 6. * xm * xm * _xySum[i][i] / nSamp
			- 3. * xm * xm * xm * xm;
		  if (nSamp < 2 || xr < 0.) xr = 0.;
		  outData[l] = xr;
		}
	    break;
	}
	break;
    }
    assert(l==_ntot);

    if (_numpoints) outData[l++] = (float) nSamp;

    static LogContext lp(LOG_VERBOSE);
    if (lp.active())
    {
        LogMessage msg;
        msg << "Covariance Sample: "
            << n_u::UTime(_tout-_periodUsecs/2).format(true,"%Y %m %d %H:%M:%S")
            << ' ';
        for (i = 0; i < _ntot; i++)
            msg << outData[i] << ' ';
    }
    zeroStats();

    _source.distribute(osamp);
}

/*
 * Send out whatever we have.
 */
void StatisticsCruncher::flush() throw()
{
    // calculate and send out the last set of statistics
    bool out = false;
    if (_tout != LONG_LONG_MIN && _nSamples) {
        for (unsigned int i=0; i < _ninvars; i++)
            if (_nSamples[i] > 0) out = true;
    }
    if (out) {
        computeStats();
        _tout += _periodUsecs;
    }
}
