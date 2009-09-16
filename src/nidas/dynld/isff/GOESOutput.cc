/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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
	SampleOutputBase(ioc),_goesXmtr(0),_xmitThread(0),
	_interrupted(false),_configid(-1),_stationNumber(0)
{
    if (getIOChannel()) {
        setName(string("GOESOutput: ") + getIOChannel()->getName());
	_goesXmtr = dynamic_cast<GOESXmtr*>(getIOChannel());
        if (!_goesXmtr) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    getName().c_str(),"output is not a GOESXmtr");
            assert(_goesXmtr);
        }
    }
}

/* copy constructor, with a new IOChannel */
GOESOutput::GOESOutput(GOESOutput& x,IOChannel*ioc):
        SampleOutputBase(x,ioc),_goesXmtr(0),_xmitThread(0),
        _interrupted(false),_configid(x._configid),
        _stationNumber(x._stationNumber)
{
    if (getIOChannel()) {
        setName(string("GOESOutput: ") + getIOChannel()->getName());
        _goesXmtr = dynamic_cast<GOESXmtr*>(getIOChannel());
        if (!_goesXmtr) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    getName().c_str(),"output is not a GOESXmtr");
            assert(_goesXmtr);
        }
    }
}

GOESOutput::~GOESOutput()
{
    cancelThread();
    joinThread();
    delete _xmitThread;
}
void GOESOutput::joinThread() throw()
{
    if (_xmitThread) {
	if (_xmitThread->isRunning()) _xmitThread->interrupt();
	try {
	    if (!_xmitThread->isJoined()) _xmitThread->join();
	}
	catch(const n_u::Exception& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    getName().c_str(),e.what());
	}
    }
}

void GOESOutput::cancelThread() throw()
{
    if (_xmitThread) {
	try {
	    if (_xmitThread->isRunning()) _xmitThread->cancel();
	}
	catch(const n_u::Exception& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    getName().c_str(),e.what());
	}
    }
}

void GOESOutput::killThread() throw()
{
    if (_xmitThread) {
	try {
	    if (_xmitThread->isRunning()) _xmitThread->kill(SIGUSR1);
	}
	catch(const n_u::Exception& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    getName().c_str(),e.what());
	}
    }
}

void GOESOutput::close() throw()
{
    cancelThread();
    joinThread();
    SampleOutputBase::close();
}

void GOESOutput::setIOChannel(IOChannel* val)
{
    SampleOutputBase::setIOChannel(val);
    if (getIOChannel()) {
        setName(string("GOESOutput: ") + getIOChannel()->getName());
	_goesXmtr = dynamic_cast<GOESXmtr*>(getIOChannel());
        if (!_goesXmtr) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    getName().c_str(),"output is not a GOESXmtr");
            assert(_goesXmtr);
        }
    }
    else _goesXmtr = 0;
}

/*
 * The requested sample is from the XML configuration, describing
 * what we're supposed to send out. It contains a "outvars"
 * parameter listing the names of the variables to send out.
 */
void GOESOutput::addRequestedSampleTag(SampleTag* tag)
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
		", sample=" << GET_SPS_ID(id) << ")";
        throw n_u::InvalidParameterException(
	    getName(),ost.str(),"has no \"outvars\" parameter");
    }

    vector<string> vnames;
    for (int i = 0; i < vparm->getLength(); i++) {
	Variable* var = new Variable();
	var->setName(vparm->getStringValue(i));
	tag->addVariable(var);
    }

    SampleOutputBase::addRequestedSampleTag(tag);
}

/*
 * This is how we're notified of what samples are incoming.
 * For the input sample id, create a mapping of where
 * the variables go in the output sample.
 */
void GOESOutput::addSourceSampleTag(const SampleTag* tag)
    throw(n_u::InvalidParameterException)
{

#ifdef DEBUG
    cerr << "GOESOutput:: addSampleTag, id=" <<
    	tag->getDSMId() << ',' <<
	tag->getSpSId() << 
	" station=" << tag->getStation() << endl;
#endif
    if (_stationNumber < 1) {
        _stationNumber = tag->getStation();
        WLOG(("%s: station number = %d from sample tag with id=%d,%d",
            getName().c_str(),tag->getDSMId(),tag->getSpSId()));
    }
    else if (_stationNumber != tag->getStation()) {
        throw n_u::InvalidParameterException(getName(),"site number",
            "inconsistent site number in input samples");

    }

    Project* project = Project::getInstance();

    if (_goesXmtr->getId() == 0) {
        const Parameter* ids = project->getParameter("goes_ids");
	if (ids->getLength() > _stationNumber)
	    _goesXmtr->setId((unsigned int)
	    	ids->getNumericValue(_stationNumber));
	else
	    n_u::Logger::getInstance()->log(LOG_ERR,
	    	"%s: goes_id not available for station number %d",
			getName().c_str(),_stationNumber);
    }
      
    if (_goesXmtr->getChannel() == 0) {
        const Parameter* chans = project->getParameter("goes_channels");
	if (chans->getLength() > _stationNumber)
	    _goesXmtr->setChannel((int) chans->getNumericValue(_stationNumber));
	else
	    n_u::Logger::getInstance()->log(LOG_ERR,
	    	"%s: goes channel number not available for station number %d",
			getName().c_str(),_stationNumber);
    }
      
    if (getXmitOffset() == 0) {
        const Parameter* offs = project->getParameter("goes_xmitOffsets");
	if (offs->getLength() > _stationNumber)
	    _goesXmtr->setXmitOffset((int) offs->getNumericValue(_stationNumber));
	else
	    n_u::Logger::getInstance()->log(LOG_ERR,
	    	"%s: goes transmit offset time not available for station number %d",
			getName().c_str(),_stationNumber);
    }

    if (_configid < 0) {
        const Parameter* cfg = project->getParameter("goes_config");
	if (cfg->getLength() > 0)
	    _configid = ((int) cfg->getNumericValue(0));
	else
	    n_u::Logger::getInstance()->log(LOG_ERR,
	    	"%s: goes_config parameter not found for project");
    }

    if (_goesXmtr->getId() == 0)
        throw n_u::InvalidParameterException(getName(),"id","invalid GOES id");

    if (_goesXmtr->getChannel() == 0)
        throw n_u::InvalidParameterException(getName(),"channel","invalid GOES channel");


    SampleOutputBase::addSourceSampleTag(tag);

    // for each variable in a Sample, a vector
    // of indices in output samples of where it should go.
    vector<vector<pair<int,int> > > varIndices;

    list<const SampleTag*> reqTags = getRequestedSampleTags();

    VariableIterator vi = tag->getVariableIterator();
    for ( ; vi.hasNext(); ) {
        const Variable* var = vi.next();
#ifdef DEBUG
	cerr << "GOESOutput::addSourceSampleTag input var=" << var->getName() << endl;
#endif

	vector<pair<int,int> > indices;
	
	list<const SampleTag*>::const_iterator si = reqTags.begin();
	for (int osindex = 0; si != reqTags.end(); ++si,osindex++) {
	    const SampleTag* rtag = *si;
	    VariableIterator vi2 = rtag->getVariableIterator();
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
    _sampleMap[tag->getId()] = varIndices;
}

/*
 * We're ready to go.
 */
SampleOutput* GOESOutput::connected(IOChannel* ochan) throw()
{
    n_u::Autolock lock(_sampleMutex);

    // report any requested variables that haven't been
    // found in the input samples
    list<const SampleTag*> reqTags = getRequestedSampleTags();

    list<const SampleTag*>::const_iterator si = reqTags.begin();
    for (int osindex = 0; si != reqTags.end(); ++si,osindex++) {
	const SampleTag* rtag = *si;
	VariableIterator vi2 = rtag->getVariableIterator();
	for (int ovindex = 0; vi2.hasNext(); ovindex++) {
	    const Variable* var = vi2.next();
	    bool found = false;
	    std::map<dsm_sample_id_t,vector<vector<pair<int,int> > > >::const_iterator
		mi = _sampleMap.begin();
	    for (; !found && mi != _sampleMap.end(); ++mi) {
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
    _maxPeriodUsec = 0;
    si = reqTags.begin();
    for (; si != reqTags.end(); ++si) {
	const SampleTag* rtag = *si;
	SampleT<float>* osamp = getSample<float>(rtag->getVariables().size());

	unsigned int periodUsec =
	    (unsigned int)rint(rtag->getPeriod()) * USECS_PER_SEC;

	// time of next sample
	n_u::UTime tnext = tnow.toUsecs() -
	    (tnow.toUsecs() % periodUsec) + periodUsec / 2;
	osamp->setTimeTag(tnext.toUsecs());
	osamp->setId(rtag->getId());

	for (unsigned int j = 0; j < rtag->getVariables().size(); j++)
	    osamp->getDataPtr()[j] = floatNAN;

	_outputSamples.push_back(osamp);
	_maxPeriodUsec = std::max((long long)_maxPeriodUsec,
		(long long)rint(rtag->getPeriod()) * USECS_PER_SEC);
    }
    joinThread();
    delete _xmitThread;
    _xmitThread = new n_u::ThreadRunnable("GOESOutput",this);
    _xmitThread->blockSignal(SIGINT);
    _xmitThread->blockSignal(SIGHUP);
    _xmitThread->blockSignal(SIGTERM);
    _xmitThread->unblockSignal(SIGUSR1);
    _xmitThread->start();

    return SampleOutputBase::connected(ochan);
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

    n_u::Autolock lock(_sampleMutex);

    std::map<dsm_sample_id_t,vector<vector<pair<int,int> > > >::const_iterator
    	mi = _sampleMap.find(samp->getId());
    if (mi == _sampleMap.end()) {
        WLOG(("sample tag ") << samp->getDSMId() << ',' <<
		samp->getSpSId() << " not found");
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
	    SampleT<float>* osamp = _outputSamples[osampi];
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
    _interrupted = true;
}

int GOESOutput::run() throw(n_u::Exception)
{
    unsigned int periodMsec = getXmitInterval() * MSECS_PER_SEC;
    unsigned int offsetMsec = getXmitOffset() * MSECS_PER_SEC;


    // When this thread wakes up - number of msecs after the
    // time of the XmitInterval.
    unsigned int wakeOffMsec = 10 * MSECS_PER_SEC;

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
    // To minimize missing this data, wakeOffMsec is 10 secs.
    // We currently have plenty of time available to transmit
    // data over GOES.  If we put more stations and more data
    // on one channel then this value for wakeOffMsec will have
    // to be reduced.

    try {
	_goesXmtr->init();
    }
    catch(const n_u::IOException& e) {
	PLOG(("%s: %s",getName().c_str(),e.what()));
	try {
	    _goesXmtr->reset();
	}
	catch(const n_u::IOException& e2) {
            WLOG(("%s: %s",getName().c_str(),e.what()));
	}
    }

    _goesXmtr->printStatus();	// no exception

    for (; !amInterrupted(); ) {
	if (nidas::core::sleepUntil(periodMsec,wakeOffMsec)) break;

	n_u::UTime tnow;
#ifdef DEBUG
	cerr << "woke, now=" << tnow.format(true,"%c") << endl;
#endif

	if (_interrupted) break;

	// lock sampleVector, make a copy of the vector of outputSamples.
	// Request new Samples from the pool and put them in outputSamples
	// for new incoming data.
	_sampleMutex.lock();
	vector<SampleT<float>*> outcopy = _outputSamples;

        list<const SampleTag*> reqTags = getRequestedSampleTags();

        list<const SampleTag*>::const_iterator si = reqTags.begin();
        for (int i = 0; si != reqTags.end(); ++si,i++) {

            const SampleTag* rtag = *si;
	    SampleT<float>* osamp =
	    	getSample<float>(rtag->getVariables().size());
	    // overflows at something over an hour
	    unsigned int periodUsec =
	    	(unsigned int)rint(rtag->getPeriod()) * USECS_PER_SEC;

	    // time of next sample
	    n_u::UTime tnext = tnow.toUsecs() -
		(tnow.toUsecs() % periodUsec) + periodUsec / 2;
	    osamp->setTimeTag(tnext.toUsecs());
	    osamp->setId(rtag->getId());

	    for (unsigned int j = 0; j < rtag->getVariables().size(); j++)
		osamp->getDataPtr()[j] = floatNAN;

	    _outputSamples[i] = osamp;
	}
	_sampleMutex.unlock();


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

	    n_u::UTime tsend(osamp->getTimeTag() + (periodMsec * USECS_PER_MSEC) / 2 +
		    offsetMsec * USECS_PER_MSEC);
	    if (tsend < tnow) {
		n_u::Logger::getInstance()->log(LOG_ERR,
		    "Bad time tag: %s, in tnow=%s,tsend=%s",
			tnow.format(true,"%c").c_str(),
			tsend.format(true,"%c").c_str());
	    }
	    else {
		try {
		    _goesXmtr->transmitData(tsend,_configid,osamp);
		}
		catch(const n_u::IOException& e) {
		    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
			    getName().c_str(),e.what());
		    try {
			_goesXmtr->reset();
		    }
		    catch(const n_u::IOException& e2) {
			n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
				getName().c_str(),e2.what());
		    }
		}
	    }
	    osamp->freeReference();
	}
	_goesXmtr->printStatus();	// no exception
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

        if (elname == "goes") {
	    IOChannel* ioc =
	    	IOChannel::createIOChannel((xercesc::DOMElement*)child);
	    ioc->fromDOMElement((xercesc::DOMElement*)child);
	    setIOChannel(ioc);
	    if (++niochan > 1)
		throw n_u::InvalidParameterException(
			"GOESOutput::fromDOMElement",
			"parse", "must have only one child element");
	}
	else if (elname == "sample") {
	    SampleTag* stag = new SampleTag();
	    stag->fromDOMElement((xercesc::DOMElement*)child);
	    addRequestedSampleTag(stag);
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
