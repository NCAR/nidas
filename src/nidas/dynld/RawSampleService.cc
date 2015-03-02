// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/Config.h>   // HAVE_PPOLL

#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/dynld/RawSampleService.h>
#include <nidas/core/DOMObjectFactory.h>
#include <nidas/dynld/raf/Aircraft.h>
#include <nidas/core/Project.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/core/SampleIOProcessor.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Process.h>
#include <nidas/util/Logger.h>

#include <iomanip>

#include <nidas/util/Logger.h>

#include <sys/prctl.h>

#ifdef HAVE_PPOLL
#include <poll.h>
#else
#include <sys/select.h>
#endif

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(RawSampleService)

RawSampleService::RawSampleService():
    DSMService("RawSampleService"),
    _pipeline(0),_workers(),_dsms(),_workerMutex(),
    _nsampsLast(), _nbytesLast(),
    _rawSorterLength(0.25), _procSorterLength(1.0),
    _rawHeapMax(5000000), _procHeapMax(5000000),
    _rawLateSampleCacheSize(0), _procLateSampleCacheSize(0)
{
}

RawSampleService::~RawSampleService()
{
    // pipeline::join() does not throw exceptions
    if (_pipeline) _pipeline->join();
    delete _pipeline;
}

/*
 * Initial schedule request.
 */
void RawSampleService::schedule(bool optionalProcessing) throw(n_u::Exception)
{
    DSMServer* server = getDSMServer();
    if (!_pipeline) _pipeline = new SamplePipeline();

    _pipeline->setRealTime(true);

    _pipeline->setRawSorterLength(getRawSorterLength());
    _pipeline->setProcSorterLength(getProcSorterLength());

    _pipeline->setRawLateSampleCacheSize(getRawLateSampleCacheSize());
    _pipeline->setProcLateSampleCacheSize(getProcLateSampleCacheSize());

    _pipeline->setRawHeapMax(getRawHeapMax());
    _pipeline->setProcHeapMax(getProcHeapMax());

    _pipeline->setHeapBlock(false);
    _pipeline->setKeepStats(true);

    // initialize pipeline with all expected SampleTags
    SensorIterator si = server->getSensorIterator();
    for ( ; si.hasNext(); ) {
        DSMSensor* sensor = si.next();
        SampleSource* src = sensor->getRawSampleSource();
        list<const SampleTag*> tags = src->getSampleTags();
        list<const SampleTag*>::const_iterator ti =  tags.begin();
        for ( ; ti != tags.end(); ++ti) {
            const SampleTag* tag = *ti;
            _pipeline->getRawSampleSource()->addSampleTag(tag);
        }

        src = sensor->getProcessedSampleSource();
        tags = src->getSampleTags();
        ti =  tags.begin();
        for ( ; ti != tags.end(); ++ti) {
            const SampleTag* tag = *ti;
            _pipeline->getProcessedSampleSource()->addSampleTag(tag);
        }
    }

    // Connect SampleIOProcessors to pipeline
    list<SampleIOProcessor*>::const_iterator oi;
    n_u::UTime tnow;
    for (oi = _processors.begin(); oi != _processors.end(); ++oi) {
        SampleIOProcessor* proc = *oi;
	if (!proc->isOptional() || optionalProcessing) {
	    try {
                // cerr << "Connecting " << proc->getName() << " to pipeline" << endl;

                // Call init function on processors, giving the current time.
                // This schedule() method is called to start real-time processing,
                // so this time is a approximate first sample time.
                // The initial purpose here is to cause SyncRecordGenerator to
                // load the current calibration coefficients which go in its header.
                // Typically other processors do nothing in their init function.
                proc->init(tnow.toUsecs());
		proc->connect(_pipeline);
	    }
	    catch(const n_u::InvalidParameterException& e) {
		n_u::Logger::getInstance()->log(LOG_ERR,
		    "%s: %s connect to %s: %s",
		    getName().c_str(),proc->getName().c_str(),
		    _pipeline->getName().c_str(),e.what());
                throw e;
	    }
	}
    }
    const list<SampleInput*>& inputs = getInputs();
    list<SampleInput*>::const_iterator li = inputs.begin();
    for ( ; li != inputs.end(); ++li) {
        SampleInput* input = *li;
        input->requestConnection(this);
    }
}

void RawSampleService::interrupt() throw()
{
    _pipeline->flush();
    _pipeline->interrupt();
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = getProcessors().begin(); pi != getProcessors().end(); ++pi) {
        SampleIOProcessor* proc = *pi;
        // Note: proc may not have been connected to begin with
        proc->disconnect(_pipeline);
    }
    DSMService::interrupt();
}

/*
 * This method is called when a SampleInput is connected.
 * It may be called multiple times as each DSM makes a connection.
 */
void RawSampleService::connect(SampleInput* input) throw()
{

    // This should have been detected in the fromDOMElement
    // where we check if the input is a RawSampleInputStream.
    assert(input->getRawSampleSource() != 0);

    // What DSM it came from
    const DSMConfig* dsm = input->getDSMConfig();

    if (dsm) {
        n_u::Logger::getInstance()->log(LOG_INFO,
            "%s (%s) has connected to %s",
            input->getName().c_str(),dsm->getName().c_str(),
            getName().c_str());
        // Tell the input what samples it is expecting, so
        // that clients can query it for sample ids.
        SensorIterator si = dsm->getSensorIterator();
        for ( ; si.hasNext(); ) {
            DSMSensor* sensor = si.next();
            input->addSampleTag(sensor->getRawSampleTag());
        }
    }
    else {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "RawSampleService: input %s does not match an address of any dsm, assuming from all",
		input->getName().c_str());
        SensorIterator si = Project::getInstance()->getSensorIterator();
        for ( ; si.hasNext(); ) {
            DSMSensor* sensor = si.next();
            input->addSampleTag(sensor->getRawSampleTag());
        }
    }

    input->setKeepStats(true);

    // pipeline does not own input. It just adds its sample clients to
    // the input.
    _pipeline->connect(input);

    // Create a Worker to handle the input.
    // Worker owns the SampleInputStream.
    Worker* worker = new Worker(this,input);
    _workerMutex.lock();
    _workers[input] = worker;
    _dsms[input] = dsm; // may be 0
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
}

/*
 * This method is called when a SampleInput is disconnected
 * (likely a DSM went down).  The run() method of the service
 * worker thread should have received an exception,
 * and so things may clean up by themselves, but we do a thread
 * interrupt here to make sure.
 */
void RawSampleService::disconnect(SampleInput* input) throw()
{
    n_u::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	input->getName().c_str(),getName().c_str());

#ifdef DEBUG
    cerr << "RawSampleService::disconnected, input=" << input <<
    	" input=" << input << endl;
#endif

    _pipeline->disconnect(input);

    // figure out the Worker for the input.
    n_u::Autolock tlock(_workerMutex);

    map<SampleInput*,Worker*>::iterator wi = _workers.find(input);
    if (wi == _workers.end()) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: can't find worker thread for input %s",
		getName().c_str(),input->getName().c_str());
        return;
    }

    Worker* worker = wi->second;
    // interrupt the worker. It is still owned by the DSMService base
    // class, and will be joined and deleted by the checkSubThreads method.
    worker->interrupt();
    _workers.erase(input);
    size_t ds = _dsms.size();
    _dsms.erase(input);
    if (_dsms.size() + 1 != ds)
        WLOG(("RawSampleService: disconnected, input not found in _dsms map, size=%d",ds));
}

RawSampleService::Worker::Worker(RawSampleService* svc, 
    SampleInput* input): Thread(svc->getName()+"Worker"),_svc(svc),_input(input)
{
    blockSignal(SIGUSR1);
}

RawSampleService::Worker::~Worker()
{
    if (_input  != _input->getOriginal()) delete _input;
}

void RawSampleService::Worker::interrupt()
{
    try {
        kill(SIGUSR1);
    }
    catch (const n_u::Exception& e) {}
}

int RawSampleService::Worker::run() throw(n_u::Exception)
{

    // get the existing signal mask
    sigset_t sigmask;
    pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
    // unblock SIGUSR1 in ppoll/pselect
    sigdelset(&sigmask,SIGUSR1);

#ifdef HAVE_PPOLL
    struct pollfd fds;
    fds.fd =  _input->getFd();
    int pollerrs = 0;
#ifdef POLLRDHUP
    fds.events = POLLIN | POLLRDHUP;
#else
    fds.events = POLLIN;
#endif
#else
    fd_set readfds;
    FD_ZERO(&readfds);
    int fd = _input->getFd();
#endif


    // Process the _input samples, use ppoll/pselect to atomically catch SIGUSR1
    try {
        _input->setNonBlocking(true);
        bool nonblocking = _input->isNonBlocking();
	for (;;) {
#ifdef HAVE_PPOLL
            int nfd = ::ppoll(&fds,1,NULL,&sigmask);
            if (nfd < 0) {
                if (errno == EINTR && isInterrupted()) break;
                throw n_u::IOException(_input->getName(),"ppoll",errno);
            }
#ifdef POLLRDHUP
            if (fds.revents & (POLLERR | POLLHUP | POLLRDHUP))
#else
            if (fds.revents & (POLLERR | POLLHUP))
#endif
            {
#ifdef POLLRDHUP
                if (fds.revents & POLLRDHUP)
                    WLOG(("%s: POLLRDHUP",_input->getName().c_str()));
#endif
                if (fds.revents & POLLERR)
                    WLOG(("%s: POLLERR",_input->getName().c_str()));
                if (fds.revents & POLLHUP)
                    WLOG(("%s: POLLHUP",_input->getName().c_str()));

                // Test code: try the socket read anyway
                if (pollerrs++ > 10) break;
            }
#else
            FD_SET(fd,&readfds);
            int nfd = ::pselect(fd+1,&readfds,NULL,NULL,NULL,&sigmask);
            if (nfd < 0) {
                if (errno == EINTR && isInterrupted()) break;
                throw n_u::IOException(_input->getName(),"pselect",errno);
            }
#endif
            // read until no data
	    while (_input->readSamples() && nonblocking);
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

    _svc->disconnect(_input);

    try {
	_input->close();
    }
    catch(const n_u::IOException& e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s: %s",
		_svc->getName().c_str(),_input->getName().c_str(),e.what());
    }

    if (!isInterrupted()) {
        DLOG(("%s: %s: requesting reconnection",
                    _svc->getName().c_str(),_input->getName().c_str()));
        _input->getOriginal()->requestConnection(_svc);
    }
    return RUN_OK;
}

void RawSampleService::printClock(ostream& ostr) throw()
{
    SampleSource* raw = _pipeline->getRawSampleSource();
    const SampleStats& stats = raw->getSampleStats();

    dsm_time_t tt = stats.getLastTimeTag();
    if (tt > 0LL)
        ostr << "<clock>" << n_u::UTime(tt).format(true,"%Y-%m-%d %H:%M:%S.%1f") << "</clock>\n";
    else
        ostr << "<clock>Not active</clock>\n";
}

void RawSampleService::printStatus(ostream& ostr,float deltat) throw()
{
    const char* oe[2] = {"odd","even"};
    int zebra = 0;

    printClock(ostr);

    ostr << "<status><![CDATA[";
    ostr << "\
<table id=status>\
<caption>dsm_server</caption>\
<thead>\
<tr>\
<th align=left>input/output</th>\
<th>latest timetag</th>\
<th>samp/sec</th>\
<th>byte/sec</th>\
<th>size<br>(MB)</th>\
<th>other status</th>\
</tr>\
</thead>\
<tbody align=right>\n";  // default alignment in table body

    _workerMutex.lock();
    std::map<SampleInput*,const DSMConfig*>::const_iterator ii =  _dsms.begin();
    for ( ; ii != _dsms.end(); ++ii) {
        SampleInput* input =  ii->first;
        const SampleStats& stats = input->getSampleStats();
        const DSMConfig* dsm = ii->second;
        if (!dsm) continue;
        ostr << 
            "<tr class=" << oe[zebra++%2] << "><td align=left>" <<
            (dsm ? dsm->getName() : "unknown") << "</td>";
        dsm_time_t tt = stats.getLastTimeTag();
        if (tt > 0LL)
            ostr << "<td>" << n_u::UTime(tt).format(true,"%Y-%m-%d&nbsp;%H:%M:%S.%1f") << "</td>";
        else
            ostr << "<td><font color=red>Not active</font></td>";
        size_t nsamps = stats.getNumSamples();
        float samplesps = (float)(nsamps - _nsampsLast[input]) / deltat;

        long long nbytes = stats.getNumBytes();
        float bytesps = (float)(nbytes - _nbytesLast[input]) / deltat;

        _nbytesLast[input] = nbytes;
        _nsampsLast[input] = nsamps;

        bool warn = fabs(bytesps) < 0.0001;
        ostr << 
            (warn ? "<td><font color=red><b>" : "<td>") <<
            fixed << setprecision(1) << samplesps <<
            (warn ? "</b></font></td>" : "</td>") <<
            (warn ? "<td><font color=red><b>" : "<td>") <<
            setprecision(0) << bytesps <<
            (warn ? "</b></font></td>" : "</td>");
        ostr << "<td></td><td></td></tr>\n";
    }
    _workerMutex.unlock();

    if (!_pipeline) return;
    // raw sorter
    SampleSource* src = _pipeline->getRawSampleSource();
    const SampleStats* stats = &src->getSampleStats();

    ostr << 
        "<tr class=" << oe[zebra++%2] << "><td align=left>" <<
        "raw sorter" << "</td>";

    dsm_time_t tt = stats->getLastTimeTag();
    if (tt > 0LL)
        ostr << "<td>" << n_u::UTime(tt).format(true,"%Y-%m-%d&nbsp;%H:%M:%S.%1f") << "</td>";
    else
        ostr << "<td><font color=red>Not active</font></td>";
    size_t nsamps = stats->getNumSamples();
    float samplesps = (float)(nsamps - _nsampsLast[src]) / deltat;

    long long nbytes = stats->getNumBytes();
    float bytesps = (float)(nbytes - _nbytesLast[src]) / deltat;

    _nbytesLast[src] = nbytes;
    _nsampsLast[src] = nsamps;

    bool warn = fabs(bytesps) < 0.0001;
    ostr << 
        (warn ? "<td><font color=red><b>" : "<td>") <<
        fixed << setprecision(1) << samplesps <<
        (warn ? "</b></font></td>" : "</td>") <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        setprecision(0) << bytesps <<
        (warn ? "</b></font></td>" : "</td>") <<
        "<td>" << setprecision(2) << _pipeline->getSorterNumRawBytes() / 1000000. << "</td>";

    ostr <<
        "<td align=left>sorter: #samps=" << _pipeline->getSorterNumRawSamples() <<
        ", maxsize=" << setprecision(0) << _pipeline->getSorterNumRawBytesMax() / 1000000. << " MB";
    size_t ndiscard = _pipeline->getNumDiscardedRawSamples();
    warn = ndiscard / deltat > 1.0;
    ostr << ",#discards=" <<
        (warn ? "<font color=red><b>" : "") <<
        ndiscard <<
        (warn ? "</b></font>" : "");
    size_t nfuture = _pipeline->getNumFutureRawSamples();
    warn = nfuture / deltat > 1.0;
    ostr <<  ",#future=" <<
        (warn ? "<font color=red><b>" : "") <<
        nfuture <<
        (warn ? "</b></font>" : "");
    ostr << "</td></tr>\n";

    // processed sorter
    src = _pipeline->getProcessedSampleSource();
    stats = &src->getSampleStats();

    ostr << 
        "<tr class=" << oe[zebra++%2] << "><td align=left>" <<
        "proc sorter" << "</td>";

    tt = stats->getLastTimeTag();
    if (tt > 0LL)
        ostr << "<td>" << n_u::UTime(tt).format(true,"%Y-%m-%d&nbsp;%H:%M:%S.%1f") << "</td>";
    else
        ostr << "<td><font color=red>Not active</font></td>";
    nsamps = stats->getNumSamples();
    samplesps = (float)(nsamps - _nsampsLast[src]) / deltat;

    nbytes = stats->getNumBytes();
    bytesps = (float)(nbytes - _nbytesLast[src]) / deltat;

    _nbytesLast[src] = nbytes;
    _nsampsLast[src] = nsamps;

    warn = fabs(bytesps) < 0.0001;
    ostr << 
        (warn ? "<td><font color=red><b>" : "<td>") <<
        fixed << setprecision(1) << samplesps <<
        (warn ? "</b></font></td>" : "</td>") <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        setprecision(0) << bytesps <<
        (warn ? "</b></font></td>" : "</td>") <<
        "<td>" << setprecision(2) << _pipeline->getSorterNumProcBytes() / 1000000. << "</td>";

    ostr <<
        "<td align=left>sorter: #samps=" << _pipeline->getSorterNumProcSamples() <<
        ", maxsize=" << setprecision(0) << _pipeline->getSorterNumProcBytesMax() / 1000000. << " MB";
    ndiscard = _pipeline->getNumDiscardedProcSamples();
    warn = ndiscard / deltat > 1.0;
    ostr << ",#discards=" <<
        (warn ? "<font color=red><b>" : "") <<
        ndiscard <<
        (warn ? "</b></font>" : "");
    nfuture = _pipeline->getNumFutureProcSamples();
    warn = nfuture / deltat > 1.0;
    ostr <<  ",#future=" <<
        (warn ? "<font color=red><b>" : "") <<
        nfuture <<
        (warn ? "</b></font>" : "");
    ostr << "</td></tr>\n";

    // print stats from SampleIOProcessors
    ProcessorIterator pi = getProcessorIterator();
    for ( ; pi.hasNext(); ) {
        SampleIOProcessor* proc = pi.next();
        proc->printStatus(ostr,deltat,zebra);
    }
    ostr << "</tbody></table>]]></status>\n";
}

/*
 * process <service class="RawSampleService"> element
 */
void RawSampleService::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{

    DSMService::fromDOMElement(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            const string& aname = attr.getName();
            const string& aval = attr.getValue();
            if (aname == "rawSorterLength" || aname == "procSorterLength") {
		float val;
		istringstream ist(aval);
		ist >> val;
		if (ist.fail()) throw n_u::InvalidParameterException(
		    string("dsm") + ": " + getName(), aname,aval);
                if (aname[0] == 'r') setRawSorterLength(val);
                else setProcSorterLength(val);
	    }
            else if (aname == "rawHeapMax" || aname == "procHeapMax") {
		int val;
		istringstream ist(aval);
		ist >> val;
		if (ist.fail()) throw n_u::InvalidParameterException(
		    string("dsm") + ": " + getName(), aname,aval);
                string smult;
		ist >> smult;
                int mult = 1;
                if (smult.length() > 0) {
                    if (smult[0] == 'K') mult = 1000;
                    else if (smult[0] == 'M') mult = 1000000;
                    else if (smult[0] == 'G') mult = 1000000000;
                }
                if (aname[0] == 'r') setRawHeapMax((size_t)val*mult);
                else setProcHeapMax((size_t)val*mult);
	    }
            else if (aname == "rawLateSampleCacheSize" || aname == "procLateSampleCacheSize") {
		unsigned int val;
		istringstream ist(aval);
		ist >> val;
		if (ist.fail()) throw n_u::InvalidParameterException(
		    string("dsm") + ": " + getName(), aname,aval);
                if (aname[0] == 'r') setRawLateSampleCacheSize(val);
                else setProcLateSampleCacheSize(val);
	    }
        }
    }
    list<SampleInput*>::iterator li = _inputs.begin();
    for ( ; li != _inputs.end(); ++li) {
        SampleInput* input = *li;
        if (!dynamic_cast<RawSampleInputStream*>(input)) {
            string iname = input->getName();
            throw n_u::InvalidParameterException("input",
                iname,"is not a RawSampleInputStream");
        }
#ifdef NEEDED
        SensorIterator si = getDSMServer()->getSensorIterator();
        for ( ; si.hasNext(); ) {
            DSMSensor* sensor = si.next();
            input->addSampleTag(sensor->getRawSampleTag());
        }
#endif
    }
}

