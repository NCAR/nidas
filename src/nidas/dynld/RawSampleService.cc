
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/dynld/RawSampleService.h>
#include <nidas/core/DOMObjectFactory.h>
#include <nidas/dynld/raf/Aircraft.h>
#include <nidas/core/Project.h>
#include <nidas/core/DSMServer.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(RawSampleService)

RawSampleService::RawSampleService():
	DSMService("RawSampleService"),merger(0)
{
}

/*
 * Copy constructor. We create a copy of the original configured
 * RawSampleService when an input has connected.
 */
RawSampleService::RawSampleService(const RawSampleService& x,
	SampleInputStream* newinput): DSMService(x,newinput),
	merger(0)
{
    // loop over x's processors
    const list<SampleIOProcessor*>& xprocs = x.getProcessors();
    list<SampleIOProcessor*>::const_iterator oi;
    for (oi = xprocs.begin(); oi != xprocs.end(); ++oi) {
        SampleIOProcessor* xproc = *oi;

	// We don't clone non-single DSM processors.
	if (!xproc->isOptional() && xproc->singleDSM()) {
	    // clone processor
	    SampleIOProcessor* proc = xproc->clone();
	    addProcessor(proc);
	}
    }
}

RawSampleService::~RawSampleService()
{

#ifdef DEBUG
    cerr << "~RawSampleService, disconnecting processors" << endl;
#endif
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = getProcessors().begin(); pi != getProcessors().end(); ++pi) {
        SampleIOProcessor* proc = *pi;
	if (!proc->isOptional()) {
#ifdef DEBUG
            cerr << "~RawSampleService, disconnecting proc " << proc->getName() << endl;
#endif
	    if (!proc->singleDSM() && merger) proc->disconnect(merger);
	    else if (input) proc->disconnect(input);
	}
    }

    delete merger;
#ifdef DEBUG
    cerr << "~RawSampleService done" << endl;
#endif
}


/*
 * Initial schedule request.
 */
void RawSampleService::schedule() throw(n_u::Exception)
{
    DSMServer* server = getDSMServer();
    merger = new SampleInputMerger;

    SensorIterator si = server->getSensorIterator();
    for ( ; si.hasNext(); ) {
        DSMSensor* sensor = si.next();
	merger->addSampleTag(sensor->getRawSampleTag());
    }

    // Connect non-single DSM processors to merger
    list<SampleIOProcessor*>::const_iterator oi;
    for (oi = processors.begin(); oi != processors.end(); ++oi) {
        SampleIOProcessor* processor = *oi;
	if (!processor->isOptional() && !processor->singleDSM()) {
	    try {
		processor->connect(merger);
	    }
	    catch(const n_u::IOException& ioe) {
		n_u::Logger::getInstance()->log(LOG_WARNING,
		    "%s: %s connect to %s: %s",
		    getName().c_str(),processor->getName().c_str(),
		    merger->getName().c_str(),ioe.what());
	    }
	}
    }
    input->requestConnection(this);
}

void RawSampleService::interrupt() throw()
{
    if (subServices.size() > 0) {
	cerr << "subServices.size()=" << subServices.size() <<
		" closing " << input->getName() << endl;
	input->close();
	if (merger) merger->removeInput(input);
    }
    DSMService::interrupt();
}
      
/*
 * This method is called when a SampleInput is connected.
 * It will be called on only the original RawSampleService,
 * not on the clones. It may be called multiple times
 * as each DSM makes a connection.
 */
void RawSampleService::connected(SampleInput* newinput) throw()
{
    // Figure out what DSM it came from

    SampleInputStream* newstream = dynamic_cast<SampleInputStream*>(newinput);

    assert(newstream);

    n_u::Inet4Address remoteAddr = newstream->getRemoteInet4Address();
    const DSMConfig* dsm = Project::getInstance()->findDSM(remoteAddr);

    if (!dsm) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "RawSampleService: connection from %s does not match an address of any. Ignoring connection.",
		remoteAddr.getHostAddress().c_str());
	return;
    }

    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s (%s) has connected to %s",
	newstream->getName().c_str(),dsm->getName().c_str(),
	getName().c_str());

    // If this is a new input make a copy of myself.
    // The copy will own the input.
    if (newstream != input) {
	cerr << newstream->getName() << " is a diff input from " <<
		input->getName() << endl;
	RawSampleService* newserv = new RawSampleService(*this,newstream);
	newserv->start();
	addSubService(newserv);
    }
    // merger does not own newstream. It just adds sample clients to it.
    merger->addInput(newstream);
}

/*
 * This method is called when a SampleInput is disconnected
 * (likely a DSM went down).  It will be called on the original
 * RawSampleService (whoever did the requestConnection).
 * The run() method of the service should have received an exception,
 * and so things may clean up by themselves, but we do a thread
 * cancel here to make sure.
 */
void RawSampleService::disconnected(SampleInput* inputx) throw()
{

    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	inputx->getName().c_str(),getName().c_str());

#ifdef DEBUG
    cerr << "RawSampleService::disconnected, inputx=" << inputx <<
    	" input=" << input << endl;

    // Figure out what DSM it came from. Not necessary, just for info.
    n_u::Inet4Address remoteAddr = inputx->getRemoteInet4Address();
    const DSMConfig* dsm = Project::getInstance()->findDSM(remoteAddr);

    cerr << "RawSampleService::disconnected, dsm=" << dsm << 
    	" getDSMConfig()=" << getDSMConfig() << endl;
#endif

    // figure out the cloned service for the input.
    // One could use a map of RawSampleService* by SampleInput*
    // but that must must stay in sync with the subServices set.
    // To make things easier, we do a brute force search 
    // of the subServices set - it is small after all.

    n_u::Synchronized autolock(subServiceMutex);

    set<DSMService*>::iterator si;
    RawSampleService* service = 0;
    for (si = subServices.begin(); si != subServices.end(); ++si) {
        RawSampleService* svc = (RawSampleService*) *si;
	if (svc->input == inputx) {
	    service = svc;
	    break;
	}
    }

    if (!service) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "%s: can't find service for input %s",
		getName().c_str(),inputx->getName().c_str());
	return;
    }

    merger->removeInput(inputx);

    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = service->getProcessors().begin();
    	pi != service->getProcessors().end(); ++pi) {
        SampleIOProcessor* proc = *pi;
	if (!proc->isOptional()) proc->disconnect(inputx);
    }

    // non-single DSM processors
    for (pi = getProcessors().begin(); pi != getProcessors().end(); ++pi) {
        SampleIOProcessor* proc = *pi;
	if (!proc->isOptional() && !proc->singleDSM())
		proc->disconnect(merger);
    }

    try {
	if (service->isRunning()) service->cancel();
    }
    catch(const n_u::Exception& e) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "Error cancelling service %s: %s",
		service->getName().c_str(),e.what());
    }
}

int RawSampleService::run() throw(n_u::Exception)
{
    input->init();		// throws n_u::IOException

    // connect processors to the input.  These are
    // single DSM processors, because this is a clone
    // of the original RawSampleService. The processor
    // then request connections to its outputs.
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = processors.begin(); pi != processors.end();
    	++pi) {
        SampleIOProcessor* processor = *pi;
	if (processor->isOptional()) continue;
	try {
	    processor->connect(input);
	}
	catch(const n_u::IOException& ioe) {
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s: connecting %s to %s: %s",
		getName().c_str(),
		input->getName().c_str(),
		processor->getName().c_str(),
		ioe.what());
	}
    }

    // Process the input samples.
    try {
	for (;;) {
	    if (isInterrupted()) break;
	    input->readSamples();
	}
    }
    catch(const n_u::EOFException& e) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "%s: %s: %s",
                getName().c_str(),input->getName().c_str(),e.what());
    }
    catch(const n_u::IOException& e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s: %s",
                getName().c_str(),input->getName().c_str(),e.what());
    }

    try {
	list<SampleIOProcessor*>::const_iterator pi;
	for (pi = processors.begin(); pi != processors.end(); ++pi) {
	    SampleIOProcessor* processor = *pi;
	    if (processor->isOptional()) continue;
	    cerr << getName() << " calling disconnect of " << input->getName() <<
	    	" on " << processor->getName() << endl;
	    processor->disconnect(input);
	}
	cerr << "closing " << input->getName() << endl;
	input->close();
    }
    catch(const n_u::IOException& e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s: %s",
		getName().c_str(),input->getName().c_str(),e.what());
    }

    return 0;
}

/*
 * process <service class="RawSampleService"> element
 */
void RawSampleService::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{

    DSMService::fromDOMElement(node);

    if (!dynamic_cast<RawSampleInputStream*>(input)) {
	string iname = input->getName();
	delete input;
	throw n_u::InvalidParameterException("input",
	    iname,"is not a RawSampleInputStream");
    }
    SensorIterator si = getDSMServer()->getSensorIterator();
    for ( ; si.hasNext(); ) {
        DSMSensor* sensor = si.next();
	input->addSampleTag(sensor->getRawSampleTag());
    }
}

xercesc::DOMElement* RawSampleService::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

xercesc::DOMElement* RawSampleService::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

