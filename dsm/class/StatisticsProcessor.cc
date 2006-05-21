/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-11 17:55:33 -0700 (Sat, 11 Feb 2006) $

    $LastChangedRevision: 3284 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/StatisticsProcessor.cc $
 ********************************************************************

*/

#include <StatisticsProcessor.h>
#include <Project.h>
#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;

CREATOR_FUNCTION(StatisticsProcessor);

StatisticsProcessor::StatisticsProcessor()
{
    setName("StatisticsProcessor");
}

/*
 * Copy constructor
 */
StatisticsProcessor::StatisticsProcessor(const StatisticsProcessor& x):
	SampleIOProcessor(x)
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
	throw(atdUtil::InvalidParameterException)
{

    const std::list<const Parameter*>& parms = tag->getParameters();
    std::list<const Parameter*>::const_iterator pi;

    const Parameter* vparm = 0;
    // bool anyStation = true;

    struct OutputInfo outputInfo;
    outputInfo.numpoints = false;
    outputInfo.type = StatisticsCruncher::STATS_UNKNOWN;

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
	else if (p->getType() == Parameter::BOOL_PARAM &&
		p->getName() == "npoints" && p->getLength() == 1) {
	    outputInfo.numpoints = p->getNumericValue(0) != 0.0;
	}
	/*
	else if (p->getType() == Parameter::BOOL_PARAM &&
		p->getName() == "anyStation" && p->getLength() == 1) {
	    anyStation = p->getNumericValue(0) != 0.0;
	}
	*/
    }
    if (!vparm) {
	ostringstream ost;
	dsm_sample_id_t id = tag->getId();
	ost << "sample id=" << id << "(dsm=" << GET_DSM_ID(id) <<
		", sample=" << GET_SHORT_ID(id) << ")";
        throw atdUtil::InvalidParameterException(
    	getName(),ost.str(),"has no \"invars\" parameter");
    }
    if (outputInfo.type == StatisticsCruncher::STATS_UNKNOWN) {
	ostringstream ost;
	dsm_sample_id_t id = tag->getId();
	ost << "sample id=" << id << "(dsm=" << GET_DSM_ID(id) <<
		", sample=" << GET_SHORT_ID(id) << ")";
        throw atdUtil::InvalidParameterException(
    	getName(),ost.str(),"has no \"type\" parameter");
    }

    if (tag->getRate() <= 0.0) {
	ostringstream ost;
	ost << "sample id=" << getId() << "(dsm=" << getDSMId() <<
		", sample=" << getShortId() << ")";
        throw atdUtil::InvalidParameterException(
	    getName(),ost.str(),"has an unknown period or rate");
    }

    vector<string> vnames;
    for (int i = 0; i < vparm->getLength(); i++) {
	Variable* var = new Variable();
	var->setName(vparm->getStringValue(i));
	// if (anyStation) var->setStation(-1);
	// else var->setStation(-2);
	tag->addVariable(var);
    }

    // save stuff that doesn't fit in the sample tag.
    infoBySampleId[tag->getId()] = outputInfo;

    configTags.push_back(tag);
}

void StatisticsProcessor::connect(SampleInput* input) throw(atdUtil::IOException)
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
	// find matches against first variable.
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
		bool match = *invar == *myvar;
		cerr << invar->getName() << '(' << invar->getStation() <<
			") == " <<
			myvar->getName() << '(' << myvar->getStation() <<
			") = " << match << endl;
#endif
		
		// first variable match
		if (*invar == *myvar) {
		    SampleTag* tmptag = new SampleTag(*mytag);
		    tmptag->setSampleId(
		    	Project::getInstance()->getUniqueSampleId(0));
		    if (myvar->getStation() < 0 && invar->getStation() >= 0)
			tmptag->setStation(invar->getStation());

		    struct OutputInfo info = infoBySampleId[mytag->getId()];
		    StatisticsCruncher* cruncher =
			new StatisticsCruncher(tmptag,info.type,info.numpoints);
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
	if (nmatches == 0) throw atdUtil::IOException(getName(),
		"connect",string("no match for variable ") + myvar->getName());
    }

    for (list<const SampleTag*>::const_iterator si = newtags.begin();
    	si != newtags.end(); ++si) 
    {
	const SampleTag* stag = *si;
        SampleIOProcessor::addSampleTag(new SampleTag(*stag));
    }

    SampleIOProcessor::connect(input);
}

void StatisticsProcessor::disconnect(SampleInput* input) throw(atdUtil::IOException)
{
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = crunchers.begin(); ci != crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
	cruncher->disconnect(input);
	cruncher->flush();
    }
    SampleIOProcessor::connect(input);
}
 
void StatisticsProcessor::connected(SampleOutput* output) throw()
{

    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = crunchers.begin(); ci != crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
	cruncher->addSampleClient(output);
    }
    SampleIOProcessor::connected(output);
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

