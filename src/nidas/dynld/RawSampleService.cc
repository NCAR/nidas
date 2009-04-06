
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

#include <iomanip>

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
    list<SampleIOProcessor*>::const_iterator pi;
    for (pi = getProcessors().begin(); pi != getProcessors().end(); ++pi) {
        SampleIOProcessor* proc = *pi;
	if (!proc->isOptional()) {
	    if (!proc->cloneOnConnection() && _merger) proc->disconnect(_merger);
	}
    }
    delete _merger;
}

/*
 * Initial schedule request.
 */
void RawSampleService::schedule() throw(n_u::Exception)
{
    DSMServer* server = getDSMServer();
    _merger = new SampleInputMerger;
    _merger->setRealTime(true);

    SensorIterator si = server->getSensorIterator();
    for ( ; si.hasNext(); ) {
        DSMSensor* sensor = si.next();
	_merger->addSampleTag(sensor->getRawSampleTag());
	SampleTagIterator ti = sensor->getSampleTagIterator();
	for ( ; ti.hasNext(); ) {
	    const SampleTag* stag = ti.next();
	    _merger->addSampleTag(stag);
	}
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
 * It may be called multiple times as each DSM makes a connection.
 */
void RawSampleService::connected(SampleInput* input) throw()
{
    // Figure out what DSM it came from
    SampleInputStream* stream =
        dynamic_cast<SampleInputStream*>(input);
    assert(stream);

    n_u::Inet4Address remoteAddr = stream->getRemoteInet4Address();
    const DSMConfig* dsm = Project::getInstance()->findDSM(remoteAddr);

    // perhaps the request came directly from one of my interfaces.
    // If so, see if there is a "localhost" dsm.
    if (!dsm) {
        n_u::Socket tmpsock;
        list<n_u::Inet4NetworkInterface> ifaces = tmpsock.getInterfaces();
        tmpsock.close();
        list<n_u::Inet4NetworkInterface>::const_iterator ii = ifaces.begin();
        for ( ; !dsm && ii != ifaces.end(); ++ii) {
            n_u::Inet4NetworkInterface iface = *ii;
            // cerr << "iface=" << iface.getAddress().getHostAddress() << endl;
            if (iface.getAddress() == remoteAddr) {
                remoteAddr = n_u::Inet4Address(INADDR_LOOPBACK);
                dsm = Project::getInstance()->findDSM(remoteAddr);
            }
        }
    }

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
    // Worker owns the SampleInputStream.
    Worker* worker = new Worker(this,stream);
    _workerMutex.lock();
    _workers[input] = worker;
    _dsms[input] = dsm;
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
 * (likely a DSM went down).  The run() method of the service
 * worker thread should have received an exception,
 * and so things may clean up by themselves, but we do a thread
 * interrupt here to make sure.
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
    _merger->flush();

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
    worker->interrupt();
    _workers.erase(input);
    size_t ds = _dsms.size();
    _dsms.erase(input);
    if (_dsms.size() + 1 != ds)
        WLOG(("RawSampleService: disconnected, input not found in _dsms map, size=%d",ds));
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

    _input->flush();
    _svc->disconnected(_input);
    for (pi = _processors.begin(); pi != _processors.end(); ++pi) {
        SampleIOProcessor* processor = *pi;
        processor->disconnect(_input);
    }

    try {
	_input->close();
    }
    catch(const n_u::IOException& e) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s: %s",
		_svc->getName().c_str(),_input->getName().c_str(),e.what());
    }
    return RUN_OK;
}

void RawSampleService::printClock(ostream& ostr) throw()
{
    dsm_time_t tt = 0;
    if (_merger) tt = _merger->getLastInputTimeTag();
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
<table id=\"RawSampleService\">\
<caption>dsm_server</caption>\
<thead>\
<tr>\
<th>input/output</th>\
<th>sample time</th>\
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
        const DSMConfig* dsm = ii->second;
        ostr << 
            "<tr class=\"" << oe[zebra++%2] << "\"><td align=left>" <<
            (dsm ? dsm->getName() : "unknown") << "</td>";
        dsm_time_t tt = input->getLastInputTimeTag();
        if (tt > 0LL)
            ostr << "<td>" << n_u::UTime(tt).format(true,"%Y-%m-%d&nbsp;%H:%M:%S.%1f") << "</td>";
        else
            ostr << "<td><font color=red>Not active</font></td>";
        size_t nsamps = input->getNumInputSamples();
        float samplesps = (float)(nsamps - _nsampsLast[input]) / deltat;

        long long nbytes = input->getNumInputBytes();
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

    ostr << 
        "<tr class=\"" << oe[zebra++%2] << "\"><td align=left>" <<
        "merge/sort" << "</td>";

    dsm_time_t tt = 0;
    if (_merger) tt = _merger->getLastInputTimeTag();
    if (tt > 0LL)
        ostr << "<td>" << n_u::UTime(tt).format(true,"%Y-%m-%d&nbsp;%H:%M:%S.%1f") << "</td>";
    else
        ostr << "<td><font color=red>Not active</font></td>";
    size_t nsamps = (_merger ? _merger->getNumInputSamples() : 0);
    float samplesps = (float)(nsamps - _nsampsLast[_merger]) / deltat;

    long long nbytes = (_merger ? _merger->getNumInputBytes() : 0);
    float bytesps = (float)(nbytes - _nbytesLast[_merger]) / deltat;

    _nbytesLast[_merger] = nbytes;
    _nsampsLast[_merger] = nsamps;

    bool warn = fabs(bytesps) < 0.0001;
    ostr << 
        (warn ? "<td><font color=red><b>" : "<td>") <<
        fixed << setprecision(1) << samplesps <<
        (warn ? "</b></font></td>" : "</td>") <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        setprecision(0) << bytesps <<
        (warn ? "</b></font></td>" : "</td>") <<
        "<td>" << setprecision(2) << _merger->getSorterNumBytes() / 1000000. << "</td>";

    ostr <<
        "<td align=left>sorter: #samps=" << _merger->getSorterNumSamples() <<
        ", maxsize=" << setprecision(0) << _merger->getSorterNumBytesMax() / 1000000. << " MB";
    size_t ndiscard = _merger->getNumDiscardedSamples();
    warn = ndiscard / deltat > 1.0;
    ostr << ",#discards=" <<
        (warn ? "<font color=red><b>" : "") <<
        ndiscard <<
        (warn ? "</b></font>" : "");
    size_t nfuture = _merger->getNumFutureSamples();
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
        proc->printStatus(ostr,deltat,oe[zebra++%2]);
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

