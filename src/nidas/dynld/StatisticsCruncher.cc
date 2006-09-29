/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-11 17:55:33 -0700 (Sat, 11 Feb 2006) $

    $LastChangedRevision: 3284 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/StatisticsProcessor.cc $
 ********************************************************************

*/

#include <nidas/dynld/StatisticsCruncher.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

StatisticsCruncher::StatisticsCruncher(const SampleTag* stag,
	statisticsType stype,string cntsName,const Site* sitex):
	countsName(cntsName),
	numpoints(countsName.length() > 0),
	crossTerms(false),
	resampler(0),
	statsType(stype),
	nwordsSuffix(0),
	outlen(0),
	tout(LONG_LONG_MIN),
	xMin(0),xMax(0),xSum(0),xySum(0),xyzSum(0),x4Sum(0),
	nSamples(0),triComb(0),
	ncov(0),ntri(0),n1mom(0),n2mom(0),n3mom (0),n4mom(0),ntot(0),
	site(sitex)

{
    switch(statsType) {
    case STATS_UNKNOWN:
    case STATS_MINIMUM:
    case STATS_MAXIMUM:
    case STATS_MEAN:
    case STATS_VAR:
        crossTerms = false;
	break;
    case STATS_COV:
    case STATS_TRIVAR:
    case STATS_PRUNEDTRIVAR:
    case STATS_FLUX:
    case STATS_RFLUX:
    case STATS_SFLUX:
        crossTerms = true;
	numpoints = true;
	break;
    }
    //
    for (VariableIterator vi = stag->getVariableIterator(); vi.hasNext(); ) {
	const Variable* vin = vi.next();
	Variable* v = new Variable(*vin);
	if (site) v->setSiteAttributes(site);
	inVariables.push_back(v);
#ifdef DEBUG
	cerr << "StatisticsCruncher, var=" << v->getName() <<
		" site=" << (site ? site->getName() : "unknown") <<
		" suffix=" << (site ? site->getSuffix() : "unknown") <<
		" site number=" << (site ? site->getNumber() : -99) << endl;
#endif
    }
    nvars = inVariables.size();
    periodUsecs = (dsm_time_t)rint(MSECS_PER_SEC / stag->getRate()) *
    	USECS_PER_MSEC;
    outSample.setSampleId(stag->getId());
    outSample.setRate(stag->getRate());
    outSample.setSiteAttributes(site);

    createCombinations();
}

StatisticsCruncher::StatisticsCruncher(const StatisticsCruncher& x):
	countsName(x.countsName),
	numpoints(x.numpoints),
	periodUsecs(x.periodUsecs),
	crossTerms(x.crossTerms),
	resampler(0),
	statsType(x.statsType),
	nwordsSuffix(0),
	outlen(0),
	tout(LONG_LONG_MIN),
	xMin(0),xMax(0),xSum(0),xySum(0),xyzSum(0),x4Sum(0),
	nSamples(0),triComb(0),
	ncov(0),ntri(0),n1mom(0),n2mom(0),n3mom (0),n4mom(0),ntot(0),
	site(x.site)

{
    vector<Variable*>::const_iterator vi;
    for (vi = x.inVariables.begin(); vi != x.inVariables.end(); ++vi) {
        Variable* var = *vi;
	inVariables.push_back(new Variable(*var));
    }
    nvars = inVariables.size();

    outSample.setSampleId(x.outSample.getSampleId());
    if (x.sampleTags.size() > 0)
    	sampleTags.insert(&outSample);

    createCombinations();
}

StatisticsCruncher::~StatisticsCruncher()
{
    map<dsm_sample_id_t,sampleInfo >::iterator vmi;
    for (vmi = sampleMap.begin(); vmi != sampleMap.end(); ++vmi) {
	struct sampleInfo& sinfo = vmi->second;
	vector<int*>& vindices = sinfo.varIndices;
	for (unsigned int iv = 0; iv < vindices.size(); iv++)
	    delete [] vindices[iv];
    }

    delete [] xSum;
    delete [] xMin;
    delete [] xMax;
    if (xySum) delete [] xySum[0];
    delete [] xySum;
    delete [] xyzSum;
    delete [] x4Sum;
    delete [] nSamples;

    if (triComb) {
	for (int i=0; i < ntri; i++) delete [] triComb[i];
	delete [] triComb;
    }

    for (unsigned int i = 0; i < inVariables.size(); i++)
    	delete inVariables[i];

    delete resampler;
}

/* static */
StatisticsCruncher::statisticsType StatisticsCruncher::getStatisticsType(const string& type)
	throw(n_u::InvalidParameterException)
{
    statisticsType stype;

    if (type == "minimum" || type == "min")		stype = STATS_MINIMUM;
    else if (type == "maximum" || type == "max") 	stype = STATS_MAXIMUM;
    else if (type == "mean")				stype = STATS_MEAN;
    else if (type == "variance" || type == "var") 	stype = STATS_VAR;
    else if (type == "covariance")			stype = STATS_COV;
    else if (type == "flux")				stype = STATS_FLUX;
    else if (type == "reducedflux")			stype = STATS_RFLUX;
    else if (type == "scalarflux")			stype = STATS_SFLUX;
    else if (type == "trivar")				stype = STATS_TRIVAR;
    else if (type == "prunedtrivar")			stype = STATS_PRUNEDTRIVAR;
    else throw n_u::InvalidParameterException(
    	"StatisticsProcessor","unrecognized type type",type);

    return stype;
}

void StatisticsCruncher::splitNames()
{
    splitVarNames.clear();
    for (unsigned int i = 0; i < inVariables.size(); i++) {
        const string& n = inVariables[i]->getName();
	vector<string> words;
	for (unsigned int cpos = 0;;) {
	    unsigned int dot = n.find('.',cpos+1);
	    if (dot == string::npos) {
	        words.push_back(n.substr(cpos));
		break;
	    }
	    words.push_back(n.substr(cpos,dot-cpos));
	    cpos = dot;
	}
	splitVarNames.push_back(words);
    }

    // compute how many trailing words the names have in common
    int nw0 = splitVarNames[0].size();
    for (nwordsSuffix = 0; nwordsSuffix < nw0 - 1; nwordsSuffix++) {
	const string& suff = splitVarNames[0][nw0-nwordsSuffix-1];
	unsigned int i; 
	for (i = 1; i < splitVarNames.size(); i++) {
	    int nw = splitVarNames[i].size();
	    if (nw < nwordsSuffix + 2) break;
	    if (splitVarNames[i][nw-nwordsSuffix-1] != suff) break;
	}
	if (i < splitVarNames.size()) break;
    }
}

string StatisticsCruncher::makeName(int i, int j, int k, int l)
{
    unsigned int n;

    string name = splitVarNames[i][0];
    if (j >= 0) {
	name += '\'';
	name += splitVarNames[j][0];
	name += '\'';
	if (k >= 0) {
	    name += splitVarNames[k][0];
	    name += '\'';
	    if (l >= 0) {
		name += splitVarNames[l][0];
		name += '\'';
	    }
	}
    }
    // middle section
    vector<string> middles;
    string middle;
    for (n = 1; n < splitVarNames[i].size() - nwordsSuffix; n++) {
	if (n == 1) middle += splitVarNames[i][n].substr(1);
	else middle += splitVarNames[i][n];
    }
    middles.push_back(middle);
    if (j >= 0) {
	middle.clear();
	for (n = 1; n < splitVarNames[j].size() - nwordsSuffix; n++) {
	    if (n == 1) middle += splitVarNames[j][n].substr(1);
	    else middle += splitVarNames[j][n];
	}
	middles.push_back(middle);
	if (k >= 0) {
	    middle.clear();
	    for (n = 1; n < splitVarNames[k].size() - nwordsSuffix; n++) {
		if (n == 1) middle += splitVarNames[k][n].substr(1);
		else middle += splitVarNames[k][n];
	    }
	    middles.push_back(middle);
	    middle.clear();
	    if (l >= 0) {
		for (n = 1; n < splitVarNames[l].size() - nwordsSuffix; n++) {
		    if (n == 1) middle += splitVarNames[l][n].substr(1);
		    else middle += splitVarNames[l][n];
		}
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
	name += string(".(");
	for (n = 0; n < middles.size(); n++) {
	    if (n > 0) name += string(",");
	    name += middles[0];
	}
	name += string(")");
    }
    // suffix
    for (n = splitVarNames[i].size() - nwordsSuffix;
	n < splitVarNames[i].size(); n++)
	name += splitVarNames[i][n];
    return name;
}
string StatisticsCruncher::makeUnits(int i, int j, int k, int l)
{
    vector<string> unitsVec;
    if (inVariables[i]->getConverter())
    	unitsVec.push_back(inVariables[i]->getConverter()->getUnits());
    else unitsVec.push_back(inVariables[i]->getUnits());

    if (j >= 0) {
	if (inVariables[j]->getConverter())
	    unitsVec.push_back(inVariables[j]->getConverter()->getUnits());
	else unitsVec.push_back(inVariables[j]->getUnits());
	if (k >= 0) {
	    if (inVariables[k]->getConverter())
		unitsVec.push_back(inVariables[k]->getConverter()->getUnits());
	    else unitsVec.push_back(inVariables[k]->getUnits());
	    if (l >= 0)
		if (inVariables[l]->getConverter())
		    unitsVec.push_back(inVariables[l]->getConverter()->getUnits());
		else unitsVec.push_back(inVariables[l]->getUnits());
	}
    }
    return makeUnits(unitsVec);
}

string StatisticsCruncher::makeUnits(const vector<string>& units)
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

void StatisticsCruncher::setupMoments(int nv,int nmoment)
{
    for (int i = 0; i < nv; i++) {
	string name;
	string units;
	switch (nmoment) {
	case 1:
	    name= makeName(i);
	    units = makeUnits(i);
	    n1mom = nv;
	    break;
	case 2:
	    name= makeName(i,i);
	    units = makeUnits(i,i);
	    n2mom = nv;
	    break;
	case 3:
	    name= makeName(i,i,i);
	    units = makeUnits(i,i,i);
	    n3mom = nv;
	    break;
	case 4:
	    name= makeName(i,i,i,i);
	    units = makeUnits(i,i,i,i);
	    n4mom = nv;
	    break;
	}

        if (outSample.getVariables().size() <= nOutVar) {
	    Variable* v = new Variable(*inVariables[i]);
	    outSample.addVariable(v);
	}
	outSample.getVariable(nOutVar).setName(name);
	outSample.getVariable(nOutVar++).setUnits(units);
    }
}

void StatisticsCruncher::setupCovariances()
{
    ncov = (nvars * (nvars + 1)) / 2;

    for (int i = 0; i < nvars; i++) {
	for (int j = i; j < nvars; j++) {
	    string name = makeName(i,j);
	    string units = makeUnits(i,j);

	    if (outSample.getVariables().size() <= nOutVar) {
		Variable* v = new Variable(*inVariables[i]);
		outSample.addVariable(v);
	    }
	    outSample.getVariable(nOutVar).setName(name);
	    outSample.getVariable(nOutVar++).setUnits(units);
	}
    }
}

void StatisticsCruncher::setupTrivariances()
{
    ntri = 0;
    for (int i = 0; i < nvars; i++) {
	for (int j = i; j < nvars; j++) {
	    for (int k = j; k < nvars; k++,ntri++) {
		string name = makeName(i,j,k);
		string units = makeUnits(i,j,k);

		if (outSample.getVariables().size() <= nOutVar) {
		    Variable* v = new Variable(*inVariables[i]);
		    outSample.addVariable(v);
		}
		outSample.getVariable(nOutVar).setName(name);
		outSample.getVariable(nOutVar++).setUnits(units);
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
 *  wss		N-3	w vs scalar-scalar (no scalar cross terms)
 *  [uv][uvw]w	5	(uvw, vuw are duplicates)
 *  [uvw]ws		3 * (N - 3)
 *
 *  total:		5 * N - 7
 */
void StatisticsCruncher::setupPrunedTrivariances()
{

    ntri = 5 * nvars - 7;

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
    triComb = new int*[ntri];
    for (int i=0; i < ntri; i++) triComb[i] = new int[3];

    int nt = 0;

    /* x^3 3rd moments */
    for (int i = 0; i < nvars; i++,nt++)
	triComb[nt][0] = triComb[nt][1] = triComb[nt][2] = i;
    setupMoments(nvars,3);

    // ws^2 trivariances
    int i = 2;
    for (int j = 3; j < nvars; j++,nt++) {
	triComb[nt][0] = i;
	triComb[nt][1] = j;
	triComb[nt][2] = j;

	string name = makeName(i,j,j);
	string units = makeUnits(i,j,j);

	if (outSample.getVariables().size() <= nOutVar) {
	    Variable* v = new Variable(*inVariables[i]);
	    outSample.addVariable(v);
	}
	outSample.getVariable(nOutVar).setName(name);
	outSample.getVariable(nOutVar++).setUnits(units);
    }
    // [uv][uvw]w trivariances
    for (int i = 0; i < 2; i++) {
	for (int j = i; j < 3; j++,nt++) {

	    triComb[nt][0] = i;
	    triComb[nt][1] = j;
	    triComb[nt][2] = 2;

	    string name = makeName(i,j,2);
	    string units = makeUnits(i,j,2);

	    if (outSample.getVariables().size() <= nOutVar) {
		Variable* v = new Variable(*inVariables[i]);
		outSample.addVariable(v);
	    }
	    outSample.getVariable(nOutVar).setName(name);
	    outSample.getVariable(nOutVar++).setUnits(units);
	}
    }
    // uws, vws, www trivariances
    for (int i = 0; i < 2; i++) {
	for (int k = 3; k < nvars; k++,nt++) {

	    triComb[nt][0] = i;
	    triComb[nt][1] = 2;
	    triComb[nt][2] = k;

	    string name = makeName(i,2,k);
	    string units = makeUnits(i,2,k);

	    if (outSample.getVariables().size() <= nOutVar) {
		Variable* v = new Variable(*inVariables[i]);
		outSample.addVariable(v);
	    }
	    outSample.getVariable(nOutVar).setName(name);
	    outSample.getVariable(nOutVar++).setUnits(units);
	}
    }
    assert(nt==ntri);
    for (nt=0; nt < ntri ; nt++)
	assert(triComb[nt][0] <= triComb[nt][1] &&
	      triComb[nt][1] <= triComb[nt][2]);

}

/*
 * covariances, including scalar variances,
 * but no scalar-scalar cross-covariances.
 */
void StatisticsCruncher::setupFluxes()
{
    ncov = 4 * nvars - 6;
    int nc = 0;
    for (int i = 0; i < nvars; i++) {
	for (int j = i; j < (i > 2 ? i + 1 : nvars); j++,nc++) {
	    string name = makeName(i,j);
	    string units = makeUnits(i,j);

	    if (outSample.getVariables().size() <= nOutVar) {
		Variable* v = new Variable(*inVariables[i]);
		outSample.addVariable(v);
	    }
	    outSample.getVariable(nOutVar).setName(name);
	    outSample.getVariable(nOutVar++).setUnits(units);
	}
    }
    assert(nc==ncov);
}

/*
 * covariances, including scalar variances,
 * but no scalar-scalar cross-covariances or variances.
 * This is typically used when computing fluxes of
 * a scalar against winds from a second sonic.
 */
void StatisticsCruncher::setupReducedFluxes()
{
    ncov = 3 * nvars - 3;	// no scalar:scalar terms
    int nc = 0;
    for (int i = 0; i < nvars && i < 3; i++) {
	for (int j = i; j < nvars; j++,nc++) {
	    string name = makeName(i,j);
	    string units = makeUnits(i,j);

	    if (outSample.getVariables().size() <= nOutVar) {
		Variable* v = new Variable(*inVariables[i]);
		outSample.addVariable(v);
	    }
	    outSample.getVariable(nOutVar).setName(name);
	    outSample.getVariable(nOutVar++).setUnits(units);
	}
    }
    assert(nc==ncov);
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
    ncov = nvars;		// covariance of first scalar 
    				// against all others

    for (int j = 0; j < nvars; j++) {
	string name = makeName(j,0);	// flip names
	string units = makeUnits(j,0);

	if (outSample.getVariables().size() <= nOutVar) {
	    Variable* v = new Variable(*inVariables[j]);
	    outSample.addVariable(v);
	}
	outSample.getVariable(nOutVar).setName(name);
	outSample.getVariable(nOutVar++).setUnits(units);
    }
}
void StatisticsCruncher::setupMinMax(const string& suffix)
{
    n1mom = nvars;

    for (int i = 0; i < nvars; i++) {
	Variable* v = inVariables[i];
	// add the suffix to the first word
	string name = splitVarNames[i][0] + suffix;
	for (unsigned int n = 1; n < splitVarNames[i].size(); n++)
	    name += splitVarNames[i][n];

	if (outSample.getVariables().size() <= nOutVar) {
	    v = new Variable(*inVariables[i]);
	    outSample.addVariable(v);
	}
	outSample.getVariable(nOutVar).setName(name);
	outSample.getVariable(nOutVar++).setUnits(makeUnits(i));
    }
}


void StatisticsCruncher::createCombinations()
{
    if (triComb) {
	for (int i=0; i < ntri; i++) delete [] triComb[i];
	delete [] triComb;
	triComb = 0;
    }

    splitNames();
    nOutVar = 0;
    ncov = ntri = n1mom = n2mom = n3mom = n4mom = 0;

    switch(statsType) {
    case STATS_MEAN:
	setupMoments(nvars,1);
	break;
    case STATS_VAR:
	setupMoments(nvars,1);
	setupMoments(nvars,2);
	break;
    case STATS_MINIMUM:
	setupMinMax("_min");
	break;
    case STATS_MAXIMUM:
	setupMinMax("_max");
	break;
    case STATS_COV:
	setupMoments(nvars,1);
	setupCovariances();
	setupMoments(nvars,3);
	setupMoments(nvars,4);
	break;
    case STATS_TRIVAR:
	setupMoments(nvars,1);
	setupCovariances();
	setupTrivariances();
	setupMoments(nvars,4);
	break;
    case STATS_PRUNEDTRIVAR:
	setupMoments(nvars,1);
	setupCovariances();
	setupPrunedTrivariances();
	setupMoments(nvars,4);
	break;
    case STATS_FLUX:
	setupMoments(nvars,1);
	setupFluxes();
	setupMoments(nvars,3);
	setupMoments(nvars,4);
	break;
    case STATS_RFLUX:
	setupMoments(3,1);	// means of winds only
	setupReducedFluxes();
	setupMoments(3,3);
	setupMoments(3,4);
	break;
    case STATS_SFLUX:
	setupMoments(1,1);	// means of scalar only
	setupReducedScalarFluxes();	// covariances of first member
	setupMoments(1,3);
	setupMoments(1,4);
	break;
    default:
	break;
    }
    ntot = n1mom + n2mom + ncov + ntri + n3mom + n4mom;
#ifdef DEBUG
    cerr << "ntot=" << ntot << " outsamp vars=" <<
    	outSample.getVariables().size() << endl;
#endif
    assert(ntot == (signed)outSample.getVariables().size());
    assert((signed)nOutVar == ntot);

#ifdef DEBUG
    cerr << "createCombinations: ";
    VariableIterator vi = outSample.getVariableIterator();
    for ( ; vi.hasNext(); ) {
	const Variable* var = vi.next();
        cerr << var->getName() << '(' << var->getStation() << ") ";
    }
    cerr << endl;
#endif
}

void StatisticsCruncher::initStats()
{
    if (numpoints && ntot == (signed)outSample.getVariables().size()) {
	Variable* v = new Variable();
	if (countsName.length() == 0) {
	    countsName = "counts";
	    // add a suffix to the counts name
	    for (unsigned int i = splitVarNames[0].size() - nwordsSuffix;
		i < splitVarNames[0].size(); i++)
		countsName += splitVarNames[0][i];
	}
	v->setName(countsName);
	v->setType(Variable::WEIGHT);
	v->setUnits("");
	if (site) v->setSiteAttributes(site);
#ifdef DEBUG
	cerr << "initStats counts, var name=" << v->getName() << 
		" station=" << v->getStation() <<
		" site=" << (site ? site->getName() : "unknown") << 
		" site number=" << (site ? site->getNumber() : -99) <<
		endl;
#endif
	outSample.addVariable(v);
    }

    outlen = outSample.getVariables().size();

    int i,n;

    if (statsType == STATS_MINIMUM) {
	delete [] xMin;
	xMin = new float[nvars];
    }
    else if (statsType == STATS_MAXIMUM) {
	delete [] xMax;
	xMax = new float[nvars];
    }
    else {
	delete [] xSum;
	xSum = new double[nvars];
    }

    nSamples = new int[nvars];

    /*
     * Create array of pointers so that xySum[i][j], for j >= i,
     * points to the right element in the sparse array.
     */
    if (xySum) delete [] xySum[0];
    delete [] xySum;
    xySum = 0;
    if (ncov > 0) {
	double *xySumArray = new double[ncov];

	n = (statsType == STATS_SFLUX ? 1 : nvars);
	xySum = new double*[n];

	for (i = 0; i < n; i++) {
	    xySum[i] = xySumArray - i;
	    if ((statsType == STATS_FLUX || statsType == STATS_RFLUX) && i > 2)
	    	xySumArray++;
	    else xySumArray += nvars - i;
	}
    }
    else if (n2mom > 0) {
	assert(n2mom == nvars);
	double *xySumArray = new double[n2mom];
	xySum = new double*[n2mom];
	for (i = 0; i < nvars; i++)
	    xySum[i] = xySumArray;
    }

    delete [] xyzSum;
    xyzSum = 0;
    if (ntri > 0) xyzSum = new double[ntri];
    else if (n3mom > 0) xyzSum = new double[n3mom];

    delete [] x4Sum;
    x4Sum = 0;
    if (n4mom > 0) x4Sum = new double[n4mom];
}

void StatisticsCruncher::zeroStats()
{
    int i;
    if (xSum) for (i = 0; i < nvars; i++) xSum[i] = 0.;
    for (i = 0; i < ncov; i++) xySum[0][i] = 0.;
    for (i = 0; i < n2mom; i++) xySum[0][i] = 0.;
    for (i = 0; i < ntri; i++) xyzSum[i] = 0.;
    for (i = 0; i < n3mom; i++) xyzSum[i] = 0.;
    for (i = 0; i < n4mom; i++) x4Sum[i] = 0.;
    if (xMin) for (i = 0; i < nvars; i++) xMin[i] = 1.e37;
    if (xMax) for (i = 0; i < nvars; i++) xMax[i] = -1.e37;
    for (i = 0; i < nvars; i++) nSamples[i] = 0;
}

void StatisticsCruncher::attach(SampleSource* src)
{
    int nvarMatch = 0;
    long dsmid = -1;
    bool oneDSM = true;

    // make a copy of src's SampleTags collection.
    list<const SampleTag*> intags(src->getSampleTags().begin(),
	src->getSampleTags().end());

    list<const SampleTag*>::const_iterator inti = intags.begin();
    for ( ; nvarMatch < nvars && inti != intags.end(); ++inti ) {
	const SampleTag* intag = *inti;
	dsm_sample_id_t id = intag->getId();

	map<dsm_sample_id_t,sampleInfo >::iterator vmi =
	    sampleMap.find(id);

	struct sampleInfo sinfo;
	struct sampleInfo* sptr = &sinfo;

	if (vmi == sampleMap.end()) sptr->weightsIndex = -1;
	else {
	    sptr = &vmi->second;
	    n_u::Logger::getInstance()->log(LOG_INFO,
		"StatisticsCruncher: multiple connections for sample id=%d (dsm:%d, sample:%d)",
		id,GET_DSM_ID(id),GET_SHORT_ID(id));
	}
		
	vector<int*>& v = sptr->varIndices;
	for (unsigned int i = 0; i < inVariables.size(); i++) {

	    // loop over variables in this input, checking
	    // for a match against one of my variable names.
	    VariableIterator vi = intag->getVariableIterator();
	    for (int iv = 0; vi.hasNext(); iv++) {
		const Variable* var = vi.next();
		if (var->getType() == Variable::WEIGHT) {
		    // cerr << "weightsIndex=" << iv << endl;
		    sptr->weightsIndex = iv;
		    continue;
		}
#ifdef DEBUG
		cerr << "var=" << var->getName() <<
			" invar=" << inVariables[i]->getName() << endl;
#endif
			
		// variable match
		if (*var == *inVariables[i]) {
		    const Site* vsite = var->getSite();
		    if (site && vsite != site) continue;
		    // paranoid check that this variable hasn't been added
		    // cerr << "match for " << var->getName() << endl;

		    unsigned int j;
		    for (j = 0; j < v.size(); j++)
			if ((unsigned)v[j][1] == i) break;
		    if (j == v.size()) {
			int* idxs = new int[2];
			idxs[0] = iv;	// input index
			idxs[1] = i;	// output index
			// if crossTerms, then all variables must
			// be in one input sample.
			if (crossTerms) assert(v.size() == i);
			v.push_back(idxs);
			if (dsmid < 0) dsmid = intag->getDSMId();
                        else if (dsmid != (signed) intag->getDSMId())
				oneDSM = false;
			nvarMatch++;
		    }
		    // copy attributes of variable
		    *inVariables[i] = *var;

#ifdef DEBUG
		    cerr << "StatisticsCruncher::attach, inVariables[" <<
		    	i << "]=" << inVariables[i]->getName() << 
			" station=" << inVariables[i]->getStation() <<
			endl;
#endif
		}
	    }
	}
	if (v.size() > 0) {
	    // Should have one input sample if cross terms
	    if (crossTerms) {
	        assert(sampleMap.size() == 0);
	        assert(v.size() == inVariables.size());
	    }
	    if (vmi == sampleMap.end()) {
	        sampleMap[id] = sinfo;
#ifdef DEBUG
		cerr << "cruncher " << this << " added id=" << id <<
		    " (" << GET_DSM_ID(id) << ',' << GET_SHORT_ID(id) <<
		    "), sampleMap.size=" << sampleMap.size() << endl;
#endif
	    }

	    // if it is a raw sample from a sensor, then
	    // sensor will be non-NULL.
	    dsm_sample_id_t sensorId = id - intag->getSampleId();
	    DSMSensor* sensor = Project::getInstance()->findSensor(sensorId);

	    if (sensor) {
		SampleInput* input = dynamic_cast<SampleInput*>(src);
		assert(input);
		input->addProcessedSampleClient(this,sensor);
	    }
	    else {
		cerr << "no sensor match, id=" << id <<
		    " (" << GET_DSM_ID(id) << ',' << GET_SHORT_ID(id) <<
		    "), sampleMap.size=" << sampleMap.size() << endl;
	        src->addSampleClient(this);
	    }
	}
    }
    if (oneDSM) outSample.setDSMId(dsmid);
    else outSample.setDSMId(0);

    sampleTags.insert(&outSample);
}

void StatisticsCruncher::connect(SampleInput* input)
	throw(n_u::IOException)
{
    if (sampleTags.size() > 0)
    	throw n_u::IOException(input->getName(),"StatisticsCruncher",
		"cannot have more than one input");

    if (!resampler) {
	SampleTagIterator inti = input->getSampleTagIterator();
	for ( ; inti.hasNext(); ) {
	    const SampleTag* intag = inti.next();
	    // loop over variables in this input, checking
	    // for a match against one of my variable names.
	    int nTagVarMatch = 0;	// variable matches within this tag
	    VariableIterator vi = intag->getVariableIterator();
	    for (int iv = 0; vi.hasNext(); iv++) {
		const Variable* var = vi.next();
		for (unsigned int i = 0; i < inVariables.size(); i++)
		    if (*var == *inVariables[i]) nTagVarMatch++;
	    }
	    // resample:
	    //	  when variables are spread across more than
	    //	  one sample AND outputs involve cross-term products
#ifdef DEBUG
	    if (nTagVarMatch > 0) cerr << "nTagVarMatch=" << nTagVarMatch <<
	    	" inVariables.size=" << inVariables.size() <<
		" crossTerms=" << crossTerms << endl;
#endif
	    if (nTagVarMatch > 0 &&
	    	nTagVarMatch < (signed) inVariables.size() && crossTerms &&
			!resampler) {
		resampler = new NearestResampler(inVariables);
	    }

	    if (nTagVarMatch == (signed) inVariables.size()) break;	// done
	}
    }

    if (resampler) {
        resampler->connect(input);
	attach(resampler);
    }
    else attach(input);

    // re-create names, since we now have actual variables.
    // The main intent is to create actual output units.
    createCombinations();
    initStats();
    zeroStats();
}

void StatisticsCruncher::disconnect(SampleInput* input)
	throw(n_u::IOException)
{
    if (resampler) {
	resampler->removeSampleClient(this);
        resampler->disconnect(input);
    }
    else {
	input->removeProcessedSampleClient(this);
	input->removeSampleClient(this);
    }
}

bool StatisticsCruncher::receive(const Sample* s) throw()
{
    assert(s->getType() == FLOAT_ST);

    const SampleT<float>* fs = static_cast<const SampleT<float>* >(s);

    dsm_sample_id_t id = fs->getId();

    map<dsm_sample_id_t,sampleInfo >::iterator vmi =
    	sampleMap.find(id);
    if (vmi == sampleMap.end()) return false;	// unrecognized sample

    dsm_time_t tt = fs->getTimeTag();
    if (tt > tout) {
	if (tout != LONG_LONG_MIN) {
	    computeStats();
	    zeroStats();
	}
	tout = tt - (tt % periodUsecs) + periodUsecs;
    }
    if (tt < tout - periodUsecs) return false;

    struct sampleInfo& sinfo = vmi->second;
    const vector<int*>& vindices = sinfo.varIndices;

    const float* inData = fs->getConstDataPtr();

    unsigned int nvarsin = std::min(vindices.size(),fs->getDataLength());

    unsigned int i,j,k;
    int vi,vj,vk,vo;
    double *xySump,*xyzSump;
    float x;
    double xy;

    int nonNANs = 0;
    if (sinfo.weightsIndex >= 0)
    	nonNANs = (int) inData[sinfo.weightsIndex];
    else if (crossTerms) {
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    if(! isnan(inData[vi])) nonNANs++;
	}
    }

#ifdef DEBUG
    n_u::UTime ut(tt);
    cerr << ut.format(true,"%Y %m %d %H:%M:%S.%6f ");
    for (i = 0; i < nvarsin; i++)
	cerr << inData[i] << ' ';
    cerr << endl;
#endif

    if (crossTerms && nonNANs < nvars) return false;

    switch (statsType) {
    case STATS_MINIMUM:
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    x = inData[vi];
	    if(! isnan(x)) {
		vo = vindices[i][1];
		if (x < xMin[vo]) xMin[vo] = x;
		nSamples[vo]++;
	    }
	}
	return true;
    case STATS_MAXIMUM:
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    x = inData[vi];
	    if(! isnan(x)) {
		vo = vindices[i][1];
		if (x > xMax[vo]) xMax[vo] = x;
		nSamples[vo]++;
	    }
	}
	return true;
    case STATS_MEAN:
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    x = inData[vi];
	    if(! isnan(x)) {
		vo = vindices[i][1];
		xSum[vo] += x;
		nSamples[vo]++;
	    }
	}
	return true;
    case STATS_VAR:
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    x = inData[vi];
	    if(! isnan(x)) {
		vo = vindices[i][1];
		xSum[vo] += x;
		xySum[vo][vo] += x * x;
		nSamples[vo]++;
	    }
	}
	return true;
    case STATS_COV:
	// cross term product, all data must be non-NAN
	xySump = xySum[0];
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    // crossterms, so: vindices[i][1] == i;
	    x = inData[vi];
	    xSum[i] += x;
	    for (j = i; j < nvarsin; j++) {
		vj = vindices[j][0];
		xy = x * inData[vj];
		*xySump++ += xy;
	    }
	    xyzSum[i] += (xy = x * x * x);
	    x4Sum[i] += xy * x;
	}
	nSamples[0]++;		// only need one nSamples
	break;
    case STATS_FLUX:
	// no scalar:scalar cross terms
	// cross term product, all data must be non-NAN
	xySump = xySum[0];
	for (i = 0; i < 3; i++) {
	    vi = vindices[i][0];
	    // crossterms, so: vindices[i][1] == i;
	    x = inData[vi];
	    xSum[i] += x;
	    for (j = i; j < nvarsin; j++) {
		vj = vindices[j][0];
		xy = x * inData[vj];
		*xySump++ += xy;
	    }
	    xyzSum[i] += (xy = x * x * x);
	    x4Sum[i] += xy * x;
	}
	for (; i < nvarsin; i++) {	// scalar means and variances
	    vi = vindices[i][0];
	    x = inData[vi];
	    xSum[i] += x;
	    *xySump++ += (xy = x * x);
	    xyzSum[i] += (xy *= x);
	    x4Sum[i] += xy * x;
	}
	nSamples[0]++;		// only need one nSamples
	break;
    case STATS_RFLUX:	
	// only wind:scalar cross terms, no scalar:scalar terms
	// cross term product, all data must be non-NAN
	xySump = xySum[0];	
	for (i = 0; i < 3; i++) {
	    vi = vindices[i][0];
	    // crossterms, so: vindices[i][1] == i;
	    x = inData[vi];
	    xSum[i] += x;
	    for (j = i; j < nvarsin; j++) {
		vj = vindices[j][0];
		xy = x * inData[vj];
		*xySump++ += xy;
	    }
	    xyzSum[i] += (xy = x * x * x);
	    x4Sum[i] += xy * x;
	}
	for (; i < nvarsin; i++) {	// scalar means
	    vi = vindices[i][0];
	    xSum[i] += inData[vi];
	}
	nSamples[0]++;		// only need one nSamples
	break;
    case STATS_SFLUX:	
	// first term is scaler
	// cross term product, all data must be non-NAN
	xySump = xySum[0];		// no wind:wind terms
	i = 0;
	vi = vindices[i][0];
	// crossterms, so: vindices[i][1] == i;
	x = inData[vi];
	for (j = i; j < nvarsin; j++) {
	    vj = vindices[j][0];
	    xSum[j] += inData[vj];
	    xy = x * inData[vj];
	    *xySump++ += xy;
	}
	xyzSum[i] += (xy = x * x * x);
	x4Sum[i] += xy * x;
	nSamples[0]++;		// only need one nSamples
#ifdef DEBUG
	if (GET_DSM_ID(id) == 1 && GET_SHORT_ID(id) == 32768) {
	    cerr << n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%6f ") <<
		    '(' << GET_DSM_ID(id) << ',' << GET_SHORT_ID(id) <<
		") " << nSamples[0] << ' ';
	    for (i = 0; i < nvarsin; i++)
		cerr << inData[i] << ' ';
	    cerr << endl;
	}
#endif
	break;
    case STATS_TRIVAR:
	// cross term product, all data must be non-NAN
	xySump = xySum[0];
	xyzSump = xyzSum;
	for (i=0; i < nvarsin; i++) {	// no scalar:scalar cross terms
	    vi = vindices[i][0];
	    // crossterms, so: vindices[i][1] == i;
	    x = inData[vi];
	    xSum[i] += x;
	    for (j = i; j < nvarsin; j++) {
		vj = vindices[j][0];
		xy = x * inData[vj];
		*xySump++ += xy;
		for (k=j; k < nvarsin; k++) {
		    vk = vindices[k][0];
		    *xyzSump++ += xy * inData[vk];
		    if (k == i) x4Sum[i] += xy * x * x;
		}
	    }
	}
	nSamples[0]++;		// only need one nSamples
	break;
    case STATS_PRUNEDTRIVAR:
	// cross term product, all data be non-NAN
	xySump = xySum[0];
	xyzSump = xyzSum;
	for (i = 0; i < nvarsin; i++) {
	    vi = vindices[i][0];
	    // crossterms, so: vindices[i][1] == i;
	    x = inData[vi];
	    xSum[i] += x;
	    for (j = i; j < nvarsin; j++) {
		vj = vindices[j][0];
		xy = x * inData[vj];
		*xySump++ += xy;
	    }
	    x4Sum[i] += x * x * x * x;
	}
	for (int n = 0; n < ntri; n++) {
	    i = triComb[n][0];
	    j = triComb[n][1];
	    k = triComb[n][2];
	    vi = vindices[i][0];
	    vj = vindices[j][0];
	    vk = vindices[k][0];
	    *xyzSump++ += (double)inData[vi] *
	    	(double)inData[vj] * (double)inData[vk];
	}
	nSamples[0]++;		// only need one nSamples
	break;
    case STATS_UNKNOWN:
        break;
    }
    return true;
}

void StatisticsCruncher::computeStats()
{
    double *xyzSump;
    int i,j,k,l,n,nx,nr;
    double x,xm,xr;

    SampleT<float>* osamp = getSample<float>(outlen);
    osamp->setTimeTag(tout - periodUsecs / 2);
    osamp->setId(outSample.getId());
    // osamp->setId(0);
    float* outData = osamp->getDataPtr();
    int nSamp = nSamples[0];

    l = 0;
    switch (statsType) {
    case STATS_MINIMUM:
	for (i=0; i < nvars; i++) {
	    if (nSamples[i] > 0) outData[l++] = xMin[i];
	    else outData[l++] = floatNAN;
	}
	break;
    case STATS_MAXIMUM:
	for (i=0; i < nvars; i++) {
	    if (nSamples[i] > 0) outData[l++] = xMax[i];
	    else outData[l++] = floatNAN;
	}
	break;
    case STATS_MEAN:
	for (i=0; i < nvars; i++) {
	    if (nSamples[i] > 0) outData[l++] = xSum[i] / nSamples[i];
	    else outData[l++] = floatNAN;
	}
	break;
    default:
	/* All other types have cross-terms and use only nSamples[0] counter */
	if (nSamp == 0) {
	    for (l=0; l < ntot; l++) outData[l] = floatNAN;
	    break;
	}

	for (i=0; i < nvars; i++) {
	    xSum[i] /= nSamp;  	// compute mean
	    if (i < n1mom) outData[l++] = (float)xSum[i];
	}

	// 2nd order
	nr = nvars;
	if (statsType == STATS_RFLUX && nvars > 3) nr = 3;
	if (statsType == STATS_SFLUX && nvars > 3) nr = 1;

	for (i = 0; i < nr; i++) {
	    // no cross terms in STATS_VAR or in STATS_FLUX for scalar:scalar terms
	    nx = (statsType == STATS_VAR || (statsType == STATS_FLUX && i > 2) ? i+1 : nvars);
	    for (j=i; j < nx; j++,l++) {
		xr = xySum[i][j] / nSamp - xSum[i] * xSum[j];
		if ((i == j && xr < 0.0) || nSamp < 2) xr = 0.0;
		outData[l] = xr;
	    }
	}

	// 3rd order
	switch (statsType) {
	case STATS_TRIVAR:
	    xyzSump = xyzSum;
	    for (i = 0; i < nvars; i++) {
		xm = xSum[i];
		for (j = i; j < nvars; j++)
		  for (k = j; k < nvars; k++,l++) {
		    xr = (((x = *xyzSump++)
			 - xm      * xySum[j][k]
			 - xSum[j] * xySum[i][k]
			 - xSum[k] * xySum[i][j]) / nSamp)
			 + ( 2. * xm * xSum[j] * xSum[k]);
		    if (nSamp < 2) xr = 0.;
		    outData[l] = xr;
		    if (k == i)
		      x4Sum[i] = x4Sum[i] / nSamp
				 - 4. * xm * x / nSamp
				 + 6. * xm * xm * xySum[i][i] / nSamp
				 - 3. * xm * xm * xm * xm;
		  }
	    }
	    for (i = 0; i < nvars; i++,l++) {
		xr = x4Sum[i];
		if (xr < 0.) xr = 0.;
		outData[l] = xr;
	    }
	    break;
	case STATS_PRUNEDTRIVAR:
	    xyzSump = xyzSum;
	    for (n = 0; n < ntri; n++,l++) {
		i = triComb[n][0];
		j = triComb[n][1];
		k = triComb[n][2];
		// When referencing xySum[i][j], j must be >= i
		xm = xSum[i];
		xr = (((x = *xyzSump++)
			- xSum[i] * xySum[j][k]
			- xSum[j] * xySum[i][k]
			- xSum[k] * xySum[i][j]) / nSamp)
			+ ( 2. * xSum[i] * xSum[j] * xSum[k]);
		if (nSamp < 2) xr = 0.;
		outData[l] = xr;
		if (i == j && j == k)
		  x4Sum[i] = x4Sum[i] / nSamp
			     - 4. * xm * x / nSamp
			     + 6. * xm * xm * xySum[i][i] / nSamp
			     - 3. * xm * xm * xm * xm;
	    }
	    for (i = 0; i < nvars; i++,l++) {
		xr = x4Sum[i];
		if (xr < 0.) xr = 0.;
		outData[l] = xr;
	    }
	    break;
	default:
	    // Third order moments
	    if (n3mom > 0)
		for (i = 0; i < nr; i++,l++) {
		  xm = xSum[i];
		  xr = xyzSum[i] / nSamp
			- 3. * xm * xySum[i][i] / nSamp
			+ 2. * xm * xm * xm;
		  if (nSamp < 2) xr = 0.;
		  outData[l] = xr;
		}

	    if (n4mom > 0)
		for (i = 0; i < nr; i++,l++) {
		  xm = xSum[i];
		  xr = x4Sum[i] / nSamp
			- 4. * xm * xyzSum[i] / nSamp
			+ 6. * xm * xm * xySum[i][i] / nSamp
			- 3. * xm * xm * xm * xm;
		  if (nSamp < 2 || xr < 0.) xr = 0.;
		  outData[l] = xr;
		}
	    break;
	}
	break;
    }
    assert(l==ntot);

    if (numpoints) outData[l++] = (float) nSamp;

#ifdef DEBUG
    cerr << "Covariance Sample: " <<
    	n_u::UTime(tout-periodUsecs*.5).format(true,"%H:%M:%S");
    for (i = 0; i < ntot; i++) cerr << outData[i] << ' ';
    cerr << '\n';
#endif

    distribute(osamp);
}

/*
 * Send out whatever we have.
 */
void StatisticsCruncher::finish() throw()
{
}
