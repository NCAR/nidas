/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/dynld/StatisticsProcessor.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(StatisticsProcessor);

StatisticsProcessor::StatisticsProcessor():_statsPeriod(0.0)
{
    setName("StatisticsProcessor");
}

/*
 * Copy constructor
 */
StatisticsProcessor::StatisticsProcessor(const StatisticsProcessor& x):
	SampleIOProcessor(x),_statsPeriod(x._statsPeriod)
{
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = x.crunchers.begin(); ci != x.crunchers.end(); ++ci) {
	StatisticsCruncher* c = *ci;
        crunchers.push_back(new StatisticsCruncher(*c));
    }
}

StatisticsProcessor::~StatisticsProcessor()
{
    for (list<SampleTag*>::const_iterator ti = configTags.begin();
    	ti != configTags.end(); ++ti) {
	delete *ti;
    }
}

StatisticsProcessor* StatisticsProcessor::clone() const
{
    return new StatisticsProcessor(*this);
}

void StatisticsProcessor::addSampleTag(SampleTag* tag)
	throw(n_u::InvalidParameterException)
{

    // We do not yet add these tags to the base class with
    // SampleIOProcessor::addSampleTag.
    if (tag->getSampleId() == 0)
	tag->setSampleId(configTags.size()+1);

    const std::list<const Parameter*>& parms = tag->getParameters();
    std::list<const Parameter*>::const_iterator pi;

    const Parameter* vparm = 0;
    // bool anyStation = true;

    struct OutputInfo outputInfo;
    outputInfo.countsName = "";
    outputInfo.type = StatisticsCruncher::STATS_UNKNOWN;
    outputInfo.higherMoments = false;

    for (pi = parms.begin(); pi != parms.end(); ++pi) {
        const Parameter* p = *pi;
	if (p->getType() == Parameter::STRING_PARAM &&
		p->getName() == "invars" && p->getLength() > 0) {
	    vparm = p;
	}
	else if (p->getType() == Parameter::STRING_PARAM &&
		p->getName() == "type" && p->getLength() == 1) {
	    outputInfo.type =
	    	StatisticsCruncher::getStatisticsType(p->getStringValue(0));
	}
	else if (p->getType() == Parameter::STRING_PARAM &&
		p->getName() == "counts" && p->getLength() == 1) {
	    outputInfo.countsName = Project::getInstance()->expandString(p->getStringValue(0));
	    
	}
	else if (p->getType() == Parameter::BOOL_PARAM &&
		p->getName() == "highmoments" && p->getLength() == 1) {
	    outputInfo.higherMoments = p->getNumericValue(0) != 0;
	}
	else throw n_u::InvalidParameterException(getName(),
		"unknown statistics parameter",p->getName());
    }
    if (!vparm) {
	ostringstream ost;
	dsm_sample_id_t id = tag->getId();
	ost << "sample id=" << id << "(dsm=" << GET_DSM_ID(id) <<
		", sample=" << GET_SHORT_ID(id) << ")";
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has no \"invars\" parameter");
    }
    if (outputInfo.type == StatisticsCruncher::STATS_UNKNOWN) {
	ostringstream ost;
	dsm_sample_id_t id = tag->getId();
	ost << "sample id=" << id << "(dsm=" << GET_DSM_ID(id) <<
		", sample=" << GET_SHORT_ID(id) << ")";
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has no \"type\" parameter");
    }

    if (tag->getRate() <= 0.0) {
	ostringstream ost;
	ost << "sample id=" << getId() << "(dsm=" << getDSMId() <<
		", sample=" << getShortId() << ")";
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has an unknown period or rate");
    }

    float sPeriod = tag->getPeriod();
    if (_statsPeriod > 0.0) {
        if (fabs(sPeriod - _statsPeriod) > 1.e-3) {
            ostringstream ost;
            ost << "average period (" << tag->getPeriod() <<
                " secs) for sample id=" <<
                getId() << '(' << getDSMId() << ',' << getShortId() << ')' <<
                " differs from period of previous sample (" <<
                _statsPeriod << " secs)";
            throw n_u::InvalidParameterException(getName(),"rate",
                ost.str());
        }
    } 
    else _statsPeriod = sPeriod;

    vector<string> vnames;
    for (int i = 0; i < vparm->getLength(); i++) {
	Variable* var = new Variable();
	var->setName(Project::getInstance()->expandString(vparm->getStringValue(i)));
	tag->addVariable(var);
    }

    // save stuff that doesn't fit in the sample tag.
    infoBySampleId[tag->getId()] = outputInfo;

    configTags.push_back(tag);
}

void StatisticsProcessor::connect(SampleInput* input) throw(n_u::IOException)
{
#ifdef DEBUG
    cerr << "StatisticsProcessor connect, #of tags=" <<
    	input->getSampleTags().size() << endl;
#endif
    list<const SampleTag*> newtags;

    list<SampleTag*>::const_iterator myti = configTags.begin();
    for ( ; myti != configTags.end(); ++myti ) {
	const SampleTag* mytag = *myti;
	if (mytag->getVariables().size() < 1) continue;
	// find all matches against first variable.
	const Variable* myvar = mytag->getVariables().front();
#ifdef DEBUG
	cerr << "StatsProc::connect, myvar=" << myvar << ' ' <<
		myvar->getName() << ' ' << myvar->getStation() << endl;
#endif
	SampleTagIterator inti = input->getSampleTagIterator();
	int nmatches = 0;
	int ninputs = 0;
	for ( ; inti.hasNext(); ninputs++) {
	    const SampleTag* intag = inti.next();
#ifdef DEBUG
	    cerr << "input next sample tag, " << intag <<
	    	" id=" << intag->getId() << " ninputs=" << ninputs <<
		" #tags=" << input->getSampleTags().size() << endl;
#endif
	    for (VariableIterator invi = intag->getVariableIterator();
	    	invi.hasNext(); ) {
		const Variable* invar = invi.next();
#ifdef DEBUG
                // if (myvar->getName() == "p.ncar.11m.vt") {
                    bool match = *invar == *myvar;
                    cerr << invar->getName() << '(' << invar->getStation() <<
                            ") == " <<
                            myvar->getName() << '(' << myvar->getStation() <<
                            ") = " << match << endl;
                // }
#endif
		
		// first variable match
		if (*invar == *myvar) {
		    SampleTag* tmptag = new SampleTag(*mytag);
		    if (tmptag->getSampleId() == 0) 
                        tmptag->setSampleId(
                            Project::getInstance()->getUniqueSampleId(0));
		    const Site* site = invar->getSite();

		    struct OutputInfo info = infoBySampleId[mytag->getId()];
		    StatisticsCruncher* cruncher =
			new StatisticsCruncher(tmptag,info.type,
				info.countsName,info.higherMoments,site);
		    crunchers.push_back(cruncher);
		    cruncher->connect(input);
		    newtags.insert(newtags.begin(),
		    	cruncher->getSampleTags().begin(),
		    	cruncher->getSampleTags().end());
		    delete tmptag;
		    nmatches++;
		}
	    }
	}
	if (nmatches == 0)
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s: no match for variable %s",
		getName().c_str(),myvar->getName().c_str());
    }

    for (list<const SampleTag*>::const_iterator si = newtags.begin();
    	si != newtags.end(); ++si) 
    {
	const SampleTag* stag = *si;
        SampleIOProcessor::addSampleTag(new SampleTag(*stag));
    }

    SampleIOProcessor::connect(input);
}

void StatisticsProcessor::disconnect(SampleInput* input) throw()
{
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = crunchers.begin(); ci != crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
	cruncher->disconnect(input);
	cruncher->flush();
    }
    SampleIOProcessor::disconnect(input);
}
 
void StatisticsProcessor::connected(SampleOutput* orig,
	SampleOutput* output) throw()
{

    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = crunchers.begin(); ci != crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
	cruncher->addSampleClient(output);
    }
    SampleIOProcessor::connected(orig,output);
}
 
void StatisticsProcessor::disconnected(SampleOutput* output) throw()
{
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = crunchers.begin(); ci != crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
	cruncher->flush();
	cruncher->removeSampleClient(output);
    }
    SampleIOProcessor::disconnected(output);
}

