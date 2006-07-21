/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-03-03 12:46:08 -0700 (Fri, 03 Mar 2006) $

    $LastChangedRevision: 3299 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:5080/svn/nids/branches/ISFF_TREX/dsm/class/GOESOutput.cc $
 ********************************************************************

*/

#include <nidas/dynld/isff/GOESOutput.h>
#include <nidas/dynld/isff/GOESXmtr.h>
#include <nidas/core/Project.h>
#include <nidas/core/Looper.h>
#include <nidas/util/Logger.h>

using namespace nidas::dynld::isff;
using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,GOESOutput)

GOESOutput::GOESOutput(IOChannel* ioc):
	SampleOutputBase(ioc),goesXmtr(0),xmitThread(0),
	interrupted(false),configid(-1)
{
    if (getIOChannel()) {
        setName(string("GOESOutput: ") + getIOChannel()->getName());
	goesXmtr = dynamic_cast<GOESXmtr*>(getIOChannel());
    }
}

/* copy constructor */
GOESOutput::GOESOutput(const GOESOutput& x):
	SampleOutputBase(x),goesXmtr(0),xmitThread(0),
	interrupted(false),configid(x.configid)
{
    if (getIOChannel()) {
        setName(string("GOESOutput: ") + getIOChannel()->getName());
	goesXmtr = dynamic_cast<GOESXmtr*>(getIOChannel());
    }
}

/* copy constructor */
GOESOutput::GOESOutput(const GOESOutput& x,IOChannel*ioc):
	SampleOutputBase(x,ioc),goesXmtr(0),xmitThread(0),
	interrupted(false),configid(x.configid)
{
    if (getIOChannel()) {
        setName(string("GOESOutput: ") + getIOChannel()->getName());
	goesXmtr = dynamic_cast<GOESXmtr*>(getIOChannel());
    }
}

GOESOutput::~GOESOutput()
{
    cancelThread();
    joinThread();
    delete xmitThread;
}
void GOESOutput::joinThread() throw()
{
    if (xmitThread) {
	if (xmitThread->isRunning()) xmitThread->interrupt();
	try {
	    if (!xmitThread->isJoined()) xmitThread->join();
	}
	catch(const n_u::Exception& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    getName().c_str(),e.what());
	}
    }
}

void GOESOutput::cancelThread() throw()
{
    if (xmitThread) {
	try {
	    if (xmitThread->isRunning()) xmitThread->cancel();
	}
	catch(const n_u::Exception& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    getName().c_str(),e.what());
	}
    }
}

void GOESOutput::close() throw()
{
    joinThread();
    SampleOutputBase::close();
}

void GOESOutput::setIOChannel(IOChannel* val)
{
    SampleOutputBase::setIOChannel(val);
    if (getIOChannel()) {
        setName(string("GOESOutput: ") + getIOChannel()->getName());
	goesXmtr = dynamic_cast<GOESXmtr*>(getIOChannel());
    }
    else goesXmtr = 0;
}

/*
 * The output sample is from the XML configuration, describing
 * what we're supposed to send out. It contains a "outvars"
 * parameter listing the names of the variables to send out.
 */
void GOESOutput::addOutputSampleTag(SampleTag* tag)
	throw(n_u::InvalidParameterException)
{

    const std::list<const Parameter*>& parms = tag->getParameters();
    std::list<const Parameter*>::const_iterator pi;

    const Parameter* vparm = 0;

    for (pi = parms.begin(); pi != parms.end(); ++pi) {
        const Parameter* p = *pi;
	if (p->getType() == Parameter::STRING_PARAM &&
		p->getName() == "outvars" && p->getLength() > 0) {
	    vparm = p;
	}
    }
    if (!vparm) {
	ostringstream ost;
	dsm_sample_id_t id = tag->getId();
	ost << "sample id=" << id << "(dsm=" << GET_DSM_ID(id) <<
		", sample=" << GET_SHORT_ID(id) << ")";
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has no \"outvars\" parameter");
    }

    vector<string> vnames;
    for (int i = 0; i < vparm->getLength(); i++) {
	Variable* var = new Variable();
	var->setName(vparm->getStringValue(i));
	tag->addVariable(var);
    }

    outputSampleTags.push_back(tag);
    constOutputSampleTags.push_back(tag);
}

/*
 * This is how we're notified of what samples are incoming.
 * For the input sample id, create a mapping of where
 * the variables go in the output sample.
 */
void GOESOutput::addSampleTag(const SampleTag* tag)
{

#ifdef DEBUG
    cerr << "GOESOutput:: addSampleTag, id=" <<
    	GET_DSM_ID(tag->getId()) << ',' <<
	GET_SHORT_ID(tag->getId()) << 
	" station=" << tag->getStation() << endl;
#endif
    stationNumber = tag->getStation();

    SampleOutputBase::addSampleTag(tag);

    // for each variable in a Sample, a vector
    // of indices in output samples of where it should go.
    vector<vector<pair<int,int> > > varIndices;

    VariableIterator vi = tag->getVariableIterator();
    for ( ; vi.hasNext(); ) {
        const Variable* var = vi.next();
#ifdef DEBUG
	cerr << "input var=" << var->getName() << endl;
#endif

	vector<pair<int,int> > indices;
	
	list<SampleTag*>::const_iterator si = outputSampleTags.begin();
	for (int osindex = 0; si != outputSampleTags.end(); ++si,osindex++) {
	    SampleTag* otag = *si;
	    VariableIterator vi2 = otag->getVariableIterator();
	    for (int ovindex = 0; vi2.hasNext(); ovindex++) {
		const Variable* var2 = vi2.next();
		if (*var2 == *var) {
		    pair<int,int> p(osindex,ovindex);
		    indices.push_back(p);
#ifdef DEBUG
		    cerr << "var match myvar=" << var2->getName() << 
		    	" inputvar=" << var->getName() << endl;
#endif
		}
#ifdef DEBUG
		else {
		    cerr << "no var match myvar=" << var2->getName() << 
		    	" inputvar=" << var->getName() << endl;
		}
#endif
	    }
	}
	varIndices.push_back(indices);
    }
    sampleMap[tag->getId()] = varIndices;
}

/*
 * We're ready to go.
 */
void GOESOutput::init() throw()
{

    Project* project = Project::getInstance();

    if (goesXmtr->getId() == 0) {
        const Parameter* ids = project->getParameter("goes_ids");
	if (ids->getLength() > stationNumber)
	    goesXmtr->setId((unsigned long)
	    	ids->getNumericValue(stationNumber));
	else
	    n_u::Logger::getInstance()->log(LOG_ERR,
	    	"%s: goes_id not available for station number %d",
			getName().c_str(),stationNumber);
    }
      
    if (goesXmtr->getChannel() == 0) {
        const Parameter* chans = project->getParameter("goes_channels");
	if (chans->getLength() > stationNumber)
	    goesXmtr->setChannel((int) chans->getNumericValue(stationNumber));
	else
	    n_u::Logger::getInstance()->log(LOG_ERR,
	    	"%s: goes channel number not available for station number %d",
			getName().c_str(),stationNumber);
    }
      
    if (getXmitOffset() == 0) {
        const Parameter* offs = project->getParameter("goes_xmitOffsets");
	if (offs->getLength() > stationNumber)
	    goesXmtr->setXmitOffset((int) offs->getNumericValue(stationNumber));
	else
	    n_u::Logger::getInstance()->log(LOG_ERR,
	    	"%s: goes transmit offset time not available for station number %d",
			getName().c_str(),stationNumber);
    }

    if (configid < 0) {
        const Parameter* cfg = project->getParameter("goes_config");
	if (cfg->getLength() > 0)
	    configid = ((int) cfg->getNumericValue(0));
	else
	    n_u::Logger::getInstance()->log(LOG_ERR,
	    	"%s: goes_config parameter not found for project");
    }

    n_u::Autolock lock(sampleMutex);

    // report any output variables that haven't been found in the input samples
    list<SampleTag*>::const_iterator si = outputSampleTags.begin();
    for (int osindex = 0; si != outputSampleTags.end(); ++si,osindex++) {
	SampleTag* otag = *si;
	VariableIterator vi2 = otag->getVariableIterator();
	for (int ovindex = 0; vi2.hasNext(); ovindex++) {
	    const Variable* var = vi2.next();
	    bool found = false;
	    std::map<dsm_sample_id_t,vector<vector<pair<int,int> > > >::const_iterator
		mi = sampleMap.begin();
	    for (; !found && mi != sampleMap.end(); ++mi) {
		const vector<vector<pair<int,int> > >& varIndices = mi->second;
		for (unsigned int i = 0; !found && i < varIndices.size(); i++) {
		    const vector<pair<int,int> >& indices = varIndices[i];
		    for (unsigned int j = 0; !found && j < indices.size(); j++) {
			int osampi = indices[j].first;
			int ovari = indices[j].second;
			if (osampi == osindex && ovari == ovindex) found = true;
		    }
		}
	    }
	    if (!found) n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"%s: variable %s not found in input samples",
			getName().c_str(),var->getName().c_str());
	}
    }
      
    n_u::UTime tnow;
    maxPeriodUsec = 0;
    si = outputSampleTags.begin();
    for (; si != outputSampleTags.end(); ++si) {
	SampleTag* otag = *si;
	SampleT<float>* osamp = getSample<float>(otag->getVariables().size());

	unsigned long periodUsec =
	    (unsigned long)rint(otag->getPeriod()) * USECS_PER_SEC;

	// time of next sample
	n_u::UTime tnext = tnow.toUsecs() -
	    (tnow.toUsecs() % periodUsec) + periodUsec / 2;
	osamp->setTimeTag(tnext.toUsecs());
	osamp->setId(otag->getId());

	for (unsigned int j = 0; j < otag->getVariables().size(); j++)
	    osamp->getDataPtr()[j] = floatNAN;

	outputSamples.push_back(osamp);
	maxPeriodUsec = std::max((long long)maxPeriodUsec,
		(long long)rint(otag->getPeriod()) * USECS_PER_SEC);
    }
    joinThread();
    delete xmitThread;
    xmitThread = new n_u::ThreadRunnable("GOESOutput",this);
    xmitThread->start();
}

bool GOESOutput::receive(const Sample* samp) 
    throw()
{
#ifdef DEBUG
    cerr << "GOESOutput::receive, tt=" <<
     	n_u::UTime(samp->getTimeTag()).format(true,"%c") << endl;
#endif

    n_u::UTime tnow;
    const SampleT<float>* isamp = static_cast<const SampleT<float>*>(samp);

    n_u::Autolock lock(sampleMutex);

    std::map<dsm_sample_id_t,vector<vector<pair<int,int> > > >::const_iterator
    	mi = sampleMap.find(samp->getId());
    if (mi == sampleMap.end()) {
        cerr << "sample tag " <<
		GET_DSM_ID(samp->getId()) << ',' <<
		GET_SHORT_ID(samp->getId()) << " not found" << endl;
	return false;
    }

    const vector<vector<pair<int,int> > >& varIndices = mi->second;
    assert(varIndices.size() == samp->getDataLength());
#ifdef DEBUG
    cerr << "varIndices.size=" << varIndices.size() << endl;
#endif

    for (unsigned int i = 0; i < varIndices.size(); i++) {
	const vector<pair<int,int> >& indices = varIndices[i];
	// cerr << "indices.size=" << indices.size() << endl;
	for (unsigned int j = 0; j < indices.size(); j++) {
	    int osampi = indices[j].first;
	    int ovari = indices[j].second;
	    // cerr << "osampi=" << osampi << " ovari=" << ovari << endl;
	    SampleT<float>* osamp = outputSamples[osampi];
	    if (::llabs(isamp->getTimeTag()-osamp->getTimeTag()) > USECS_PER_MSEC) {
		// complain about a late sample, but send the data anyway.
		const char* ttmsg = "Bad";
		if (isamp->getTimeTag() == osamp->getTimeTag() -
		    getXmitInterval() * USECS_PER_SEC) {
		    ttmsg = "Late";
		    osamp->getDataPtr()[ovari] = isamp->getConstDataPtr()[i];
		}
		if (i == 0)
		    n_u::Logger::getInstance()->log(LOG_ERR,
			"%s time tag: %s, expected: %s",ttmsg,
			n_u::UTime(samp->getTimeTag()).format(true,"%c").c_str(),
			n_u::UTime(osamp->getTimeTag()).format(true,"%c").c_str());
	    }
	    else osamp->getDataPtr()[ovari] = isamp->getConstDataPtr()[i];
	}
    }
    return true;
}

void GOESOutput::interrupt()
{
    interrupted = true;
}

int GOESOutput::run() throw(n_u::Exception)
{
    unsigned int periodUsec = getXmitInterval() * USECS_PER_SEC;
    unsigned int offsetUsec = getXmitOffset() * USECS_PER_SEC;


    // When this thread wakes up - number of usecs after the
    // time of the XmitInterval.
    unsigned int wakeOffUsec = 10 * USECS_PER_SEC;

    // The input samples to GOESOutput typically come
    // from the StatisticsProcessor.  The samples out of
    // StatisticsProcessor are pushed by input
    // data timetags - not by the current clock time. So
    // the output of averaged samples for a statistics group
    // from StatisticsProcessor is delayed by deltaT of the
    // input samples of the group.
    // 
    // If the max sampling rate for the variables in a statistics
    // group is low (e.g. radiation data with a deltaT of 5 sec)
    // this delay can be quite long.
    // To minimize missing this data, wakeOffUsec is 10 secs.
    // We currently have plenty of time available to transmit
    // data over GOES.  If we put more stations and more data
    // on one channel then this value for wakeOffUsec will have
    // to be reduced.

    for (; !interrupted; ) {
	if (Looper::sleepUntil(periodUsec,wakeOffUsec)) break;

	n_u::UTime tnow;
#ifdef DEBUG
	cerr << "woke, now=" << tnow.format(true,"%c") << endl;
#endif

	if (interrupted) break;

	try {
	    goesXmtr->checkId();
	    goesXmtr->checkClock();
	}
	catch(const n_u::IOException& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    getName().c_str(),e.what());
	    goesXmtr->printStatus();	// no exception
            try {
		goesXmtr->reset();
	    }
	    catch(const n_u::IOException& e2) {
	    }
	    continue;
	}

	// lock sampleVector, make a copy of the vector of outputSamples.
	// Request new Samples from the pool and put them in outputSamples
	// for new incoming data.
	sampleMutex.lock();
	vector<SampleT<float>*> outcopy = outputSamples;

	list<SampleTag*>::const_iterator si = outputSampleTags.begin();
	for (int i = 0; si != outputSampleTags.end(); ++si,i++) {
	    SampleTag* otag = *si;
	    SampleT<float>* osamp =
	    	getSample<float>(otag->getVariables().size());
	    unsigned long periodUsec =
	    	(unsigned long)rint(otag->getPeriod()) * USECS_PER_SEC;

	    // time of next sample
	    n_u::UTime tnext = tnow.toUsecs() -
		(tnow.toUsecs() % periodUsec) + periodUsec / 2;
	    osamp->setTimeTag(tnext.toUsecs());
	    osamp->setId(otag->getId());

	    for (unsigned int j = 0; j < otag->getVariables().size(); j++)
		osamp->getDataPtr()[j] = floatNAN;

	    outputSamples[i] = osamp;
	}
	sampleMutex.unlock();


	// send out the samples in outcopy
	for (unsigned int i = 0; i <  outcopy.size(); i++) {
	    SampleT<float>* osamp = outcopy[i];
#ifdef DEBUG
	    cerr << "tsamp=" <<
		    n_u::UTime(osamp->getTimeTag()).format(true,"%c") <<
		    endl;
	    for (unsigned int j = 0; j < osamp->getDataLength(); j++)
	    	cerr << osamp->getConstDataPtr()[j] << ' ';
	    cerr << endl;
#endif

	    n_u::UTime tsend(osamp->getTimeTag() + periodUsec / 2 +
		    offsetUsec);
	    if (tsend < tnow) {
		n_u::Logger::getInstance()->log(LOG_ERR,
		    "Bad time tag: %s, in tnow=%s,tsend=%s",
			tnow.format(true,"%c").c_str(),
			tsend.format(true,"%c").c_str());
	    }
	    else {
		try {
		    goesXmtr->transmitData(tsend,configid,osamp);
		}
		catch(const n_u::IOException& e) {
		    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
			    getName().c_str(),e.what());
		    try {
			goesXmtr->reset();
		    }
		    catch(const n_u::IOException& e2) {
			n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
				getName().c_str(),e2.what());
		    }
		}
	    }
	    osamp->freeReference();
	}
	goesXmtr->printStatus();	// no exception
    }

    return n_u::Thread::RUN_OK;
}

void GOESOutput::fromDOMElement(const xercesc::DOMElement* node)
        throw(n_u::InvalidParameterException)
{
    // process <goes> tag
    int niochan = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

        if (!elname.compare("goes")) {
	    IOChannel* ioc =
	    	IOChannel::createIOChannel((xercesc::DOMElement*)child);
	    ioc->fromDOMElement((xercesc::DOMElement*)child);
	    setIOChannel(ioc);
	    if (++niochan > 1)
		throw n_u::InvalidParameterException(
			"GOESOutput::fromDOMElement",
			"parse", "must have only one child element");
	}
	else if (!elname.compare("sample")) {
	    SampleTag* stag = new SampleTag();
	    stag->fromDOMElement((xercesc::DOMElement*)child);
	    addOutputSampleTag(stag);
	}
	else throw n_u::InvalidParameterException(
                    "GOESOutput::fromDOMElement",
		    "parse", "only supports goes and sample elements");

    }
    if (!getIOChannel())
        throw n_u::InvalidParameterException(
                "GOESOutput::fromDOMElement",
                "parse", "must have one child element");
    setName(string("GOESOutput: ") + getIOChannel()->getName());
}
