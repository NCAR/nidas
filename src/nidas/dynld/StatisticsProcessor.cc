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
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(StatisticsProcessor);

StatisticsProcessor::StatisticsProcessor():
    SampleIOProcessor(false),_statsPeriod(0.0)
{
    setName("StatisticsProcessor");
}

StatisticsProcessor::~StatisticsProcessor()
{
}

void StatisticsProcessor::addRequestedSampleTag(SampleTag* tag)
	throw(n_u::InvalidParameterException)
{

    // This is a wierd sample. It doesn't contain any
    // <variable> tags, but does contain a parameter
    // called "invars" containing the variable names
    // that are to be processed.
    //
    // The number of actual output SampleTags from this processor
    // may be greater than the number of SampleTags added
    // here. For example: if a SampleTag is added here
    // with a variable called "P", then output SampleTags
    // will be created for every "P" from every station.

    const std::list<const Parameter*>& parms = tag->getParameters();
    std::list<const Parameter*>::const_iterator pi;

    const Parameter* vparm = 0;

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
	ost << "sample id=" << GET_DSM_ID(id) << ',' << GET_SPS_ID(id);
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has no \"invars\" parameter");
    }
    if (outputInfo.type == StatisticsCruncher::STATS_UNKNOWN) {
	ostringstream ost;
	dsm_sample_id_t id = tag->getId();
	ost << "sample id=" << GET_DSM_ID(id) << ',' << GET_SPS_ID(id);
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has no \"type\" parameter");
    }

    if (tag->getRate() <= 0.0) {
	ostringstream ost;
	dsm_sample_id_t id = tag->getId();
	ost << "sample id=" << GET_DSM_ID(id) << ',' << GET_SPS_ID(id);
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has an unknown period or rate");
    }

    float sPeriod = tag->getPeriod();
    if (_statsPeriod > 0.0) {
        if (fabs(sPeriod - _statsPeriod) > 1.e-3) {
            ostringstream ost;
            dsm_sample_id_t id = tag->getId();
            ost << "average period (" << tag->getPeriod() <<
                " secs) for sample id=" << GET_DSM_ID(id) << ',' << GET_SPS_ID(id) <<
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

    if (tag->getSampleId() == 0)
	tag->setSampleId(getSampleId() + getSampleTags().size() + 1);

    // save stuff that doesn't fit in the sample tag.
    _infoBySampleId[tag->getId()] = outputInfo;

    SampleIOProcessor::addRequestedSampleTag(tag);
}

void StatisticsProcessor::connect(SampleSource* source) throw()
{
#ifdef DEBUG
    cerr << "StatisticsProcessor connect, #of tags=" <<
    	source->getSampleTags().size() << endl;
#endif
    list<const SampleTag*> newtags;

    source = source->getProcessedSampleSource();
    assert(source);

    // loop over requested sample tags
    list<const SampleTag*> reqtags = getSampleTags();
    list<const SampleTag*>::const_iterator myti = reqtags.begin();
    for ( ; myti != reqtags.end(); ++myti ) {
	const SampleTag* mytag = *myti;

        // make sure we have at least one variable
	if (mytag->getVariables().size() < 1) continue;

	// find all matches against first requested variable
        // in each requested statistics sample
	const Variable* myvar = mytag->getVariables().front();
#ifdef DEBUG
	cerr << "StatsProc::connect, myvar=" << myvar << ' ' <<
		myvar->getName() << ' ' << myvar->getStation() << endl;
#endif
        list<const SampleTag*> ptags = source->getSampleTags();
        list<const SampleTag*>::const_iterator inti =  ptags.begin();
	int nmatches = 0;
	int ninputs = 0;
	for ( ; inti != ptags.end(); ++inti,ninputs++) {
	    const SampleTag* intag = *inti;
#ifdef DEBUG
	    cerr << "input next sample tag, " << intag <<
	    	" id=" << intag->getId() << " ninputs=" << ninputs <<
		" #tags=" << source->getSampleTags().size() << endl;
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
		
		// first variable match. Create a StatisticsCruncher.
		if (*invar == *myvar) {
		    const Site* site = invar->getSite();
		    struct OutputInfo info = _infoBySampleId[mytag->getId()];
		    StatisticsCruncher* cruncher =
			new StatisticsCruncher(mytag,info.type,
				info.countsName,info.higherMoments,site);

                    _connectionMutex.lock();
		    _crunchers.push_back(cruncher);
                    _connectionMutex.unlock();

		    cruncher->connect(source);
		    list<const SampleTag*> tags = cruncher->getSampleTags();
		    newtags.insert(newtags.begin(),tags.begin(),tags.end());
		    nmatches++;
		}
	    }
	}
	if (nmatches == 0)
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s: no match for variable %s",
		getName().c_str(),myvar->getName().c_str());
    }

    _connectionMutex.lock();
    // on first SampleSource connection, request output connections
    //
    // Currently this code does not support connections from more than one
    // SampleSource. This is not an issue since the source will always
    // be one SamplePipeline which is a merged SampleSource.
    // If we did have more than one SampleSource, then the
    // number of StatisticsCrunchers may grow with each SampleSource
    // connection, and then the list of SampleTags that will be output will
    // also grow. However we need to support the NetcdfRPCOutput which
    // requires that it knows all its output SampleTags before
    // it connects.

    if (_connectedSources.size() == 0) {
        const list<SampleOutput*>& outputs = getOutputs();
        list<SampleOutput*>::const_iterator oi = outputs.begin();
        for ( ; oi != outputs.end(); ++oi) {
            SampleOutput* output = *oi;
            output->addSourceSampleTags(newtags);
            SampleOutputRequestThread::getInstance()->addConnectRequest(output,this,0);
        }
    }
    _connectedSources.insert(source);
    _connectionMutex.unlock();
}

void StatisticsProcessor::disconnect(SampleSource* source) throw()
{
    source = source->getProcessedSampleSource();

    _connectionMutex.lock();
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = _crunchers.begin(); ci != _crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
	cruncher->disconnect(source);
	cruncher->flush();
    }
    _connectedSources.erase(source);
    _connectionMutex.unlock();
}
 
void StatisticsProcessor::connect(SampleOutput* output) throw()
{

    _connectionMutex.lock();
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = _crunchers.begin(); ci != _crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
	cruncher->addSampleClient(output);
    }
    _connectedOutputs.insert(output);
    _connectionMutex.unlock();
}
 
void StatisticsProcessor::disconnect(SampleOutput* output) throw()
{

    _connectionMutex.lock();
    list<StatisticsCruncher*>::const_iterator ci;
    for (ci = _crunchers.begin(); ci != _crunchers.end(); ++ci) {
        StatisticsCruncher* cruncher = *ci;
	cruncher->flush();
	cruncher->removeSampleClient(output);
    }
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

