
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
#include <nidas/util/Process.h>

#include <nidas/util/Logger.h>

#include <sys/prctl.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(RawSampleService)

RawSampleService::RawSampleService():
	DSMService("RawSampleService"),_merger(0)
{
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
	    if (!proc->cloneOnConnection() && _merger) proc->disconnect(_merger);
	}
    }

    delete _merger;
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
    _merger = new SampleInputMerger;

    SensorIterator si = server->getSensorIterator();
    for ( ; si.hasNext(); ) {
        DSMSensor* sensor = si.next();
	_merger->addSampleTag(sensor->getRawSampleTag());
    }

    // Connect non-cloned SampleIOProcessors to merger
    list<SampleIOProcessor*>::const_iterator oi;
    for (oi = _processors.begin(); oi != _processors.end(); ++oi) {
        SampleIOProcessor* processor = *oi;
	if (!processor->isOptional() && !processor->cloneOnConnection()) {
	    try {
		processor->connect(_merger);
	    }
	    catch(const n_u::IOException& ioe) {
		n_u::Logger::getInstance()->log(LOG_WARNING,
		    "%s: %s connect to %s: %s",
		    getName().c_str(),processor->getName().c_str(),
		    _merger->getName().c_str(),ioe.what());
	    }
	}
    }
    list<SampleInputStream*>::iterator li = _inputs.begin();
    for ( ; li != _inputs.end(); ++li) {
        SampleInputStream* input = *li;
        input->requestConnection(this);
    }
}

/*
 * This method is called when a SampleInput is connected.
 * It will be called on only the original RawSampleService,
 * not on the clones. It may be called multiple times
 * as each DSM makes a connection.
 */
void RawSampleService::connected(SampleInput* input) throw()
{
    // Figure out what DSM it came from
    SampleInputStream* stream =
        dynamic_cast<SampleInputStream*>(input);
    assert(stream);

    n_u::Inet4Address remoteAddr = stream->getRemoteInet4Address();
    const DSMConfig* dsm = Project::getInstance()->findDSM(remoteAddr);

    if (!dsm) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "RawSampleService: connection from %s does not match an address of any dsm. Ignoring connection.",
		remoteAddr.getHostAddress().c_str());
	stream->close();
        delete stream;
	return;
    }

    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s (%s) has connected to %s",
	stream->getName().c_str(),dsm->getName().c_str(),
	getName().c_str());

    // Create a Worker to handle the input.
    // Worker owns the RawSampleStream.
    Worker* worker = new Worker(this,stream);
    _workerMutex.lock();
    _workers[stream] = worker;
    _workerMutex.unlock();

    try {
        worker->setThreadScheduler(getSchedPolicy(),getSchedPriority());
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s: %s", getName().c_str(),e.what()));
    }

    try {
        worker->start();
    }
    catch (const n_u::Exception& e) {
        WLOG(("%s: %s", getName().c_str(),e.what()));
    }

    addSubThread(worker);

    // merger does not own stream. It just adds sample clients to it.
    _merger->addInput(stream);
}

/*
 * This method is called when a SampleInput is disconnected
 * (likely a DSM went down).  It will be called on the original
 * RawSampleService (whoever did the requestConnection).
 * The run() method of the service should have received an exception,
 * and so things may clean up by themselves, but we do a thread
 * cancel here to make sure.
 */
void RawSampleService::disconnected(SampleInput* input) throw()
{

    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	input->getName().c_str(),getName().c_str());

#ifdef DEBUG
    cerr << "RawSampleService::disconnected, input=" << input <<
    	" input=" << input << endl;

    // Figure out what DSM it came from. Not necessary, just for info.
    n_u::Inet4Address remoteAddr = input->getRemoteInet4Address();
    const DSMConfig* dsm = Project::getInstance()->findDSM(remoteAddr);

    cerr << "RawSampleService::disconnected, dsm=" << dsm << endl;
#endif

    _merger->removeInput(input);

    // figure out the Worker for the input.
    _workerMutex.lock();
    map<SampleInput*,Worker*>::iterator wi = _workers.find(input);
    if (wi == _workers.end()) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "%s: can't find worker thread for input %s",
		getName().c_str(),input->getName().c_str());
        _workerMutex.unlock();
        return;
    }

    Worker* worker = wi->second;
    worker->interrupt();
    _workers.erase(input);
    _workerMutex.unlock();
}
RawSampleService::Worker::Worker(RawSampleService* svc, 
    SampleInputStream* input): Thread(svc->getName()),_svc(svc),_input(input)
{
    blockSignal(SIGHUP);
    blockSignal(SIGINT);
    blockSignal(SIGTERM);

    // loop over svc's processors
    const list<SampleIOProcessor*>& procs = _svc->getProcessors();
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = procs.begin(); pi != procs.end(); ++pi) {
        SampleIOProcessor* proc = *pi;
	// We don't clone non-single DSM processors.
	if (!proc->isOptional() && proc->cloneOnConnection()) {
	    // clone processor
	    SampleIOProcessor* nproc = proc->clone();
            _processors.push_back(nproc);
	}
    }
}

RawSampleService::Worker::~Worker()
{
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = _processors.begin(); pi != _processors.end(); ++pi) {
        SampleIOProcessor* processor = *pi;
        delete processor;
    }
    delete _input;
}

int RawSampleService::Worker::run() throw(n_u::Exception)
{
    _input->init();		// throws n_u::IOException

    // connect workers processors to the input.
    // The processor then request connections to its outputs.
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = _processors.begin(); pi != _processors.end(); ++pi) {
        SampleIOProcessor* processor = *pi;
	try {
	    processor->connect(_input);
	}
	catch(const n_u::IOException& ioe) {
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s: connecting %s to %s: %s",
		_svc->getName().c_str(),
		_input->getName().c_str(),
		processor->getName().c_str(),
		ioe.what());
	}
    }

    // Process the _input samples.
    try {
	for (;;) {
	    if (isInterrupted()) break;
	    _input->readSamples();
	}
    }
    catch(const n_u::EOFException& e) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "%s: %s: %s",
                _svc->getName().c_str(),_input->getName().c_str(),e.what());
    }
    catch(const n_u::IOException& e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s: %s",
                _svc->getName().c_str(),_input->getName().c_str(),e.what());
    }
    try {
	list<SampleIOProcessor*>::const_iterator pi;
	for (pi = _processors.begin(); pi != _processors.end(); ++pi) {
	    SampleIOProcessor* processor = *pi;
	    processor->disconnect(_input);
	}
	cerr << "closing " << _input->getName() << endl;
	_input->close();
    }
    catch(const n_u::IOException& e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s: %s",
		_svc->getName().c_str(),_input->getName().c_str(),e.what());
    }
    return RUN_OK;
}

/*
 * process <service class="RawSampleService"> element
 */
void RawSampleService::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{

    DSMService::fromDOMElement(node);
    list<SampleInputStream*>::iterator li = _inputs.begin();
    for ( ; li != _inputs.end(); ++li) {
        SampleInputStream* input = *li;

        if (!dynamic_cast<RawSampleInputStream*>(input)) {
            string iname = input->getName();
            throw n_u::InvalidParameterException("input",
                iname,"is not a RawSampleInputStream");
        }
        SensorIterator si = getDSMServer()->getSensorIterator();
        for ( ; si.hasNext(); ) {
            DSMSensor* sensor = si.next();
            input->addSampleTag(sensor->getRawSampleTag());
        }
    }
}

