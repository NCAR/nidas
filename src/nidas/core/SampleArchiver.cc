/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/SampleArchiver.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/SampleOutputRequestThread.h>

#include <nidas/util/Logger.h>

#include <algorithm>
#include <iomanip>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleArchiver::SampleArchiver(): SampleIOProcessor(true),
    _lastFileSetState(""),_lastZebra(0),
    _connectionMutex(),_connectedSources(),_connectedOutputs(),
    _filesets(),_filesetMutex(),
    _nsampsLast(0),_nbytesLast(0),_nbytesLastByFileSet(),
    _rawArchive(true)
{
    setName("SampleArchiver");
}

SampleArchiver::~SampleArchiver()
{
    _connectionMutex.lock();
    set<SampleOutput*>::const_iterator oi = _connectedOutputs.begin();
    for ( ; oi != _connectedOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        set<SampleSource*>::const_iterator si = _connectedSources.begin();
        for ( ; si != _connectedSources.end(); ++si) {
            SampleSource* source = *si;
            source->removeSampleClient(output);
        }
        try {
            output->finish();
        }
        catch (const n_u::IOException& ioe) {
        }
        try {
            output->close();
        }
        catch (const n_u::IOException& ioe) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                "DSMEngine: error closing %s: %s",
                    output->getName().c_str(),ioe.what());
        }

        SampleOutput* orig = output->getOriginal();

        if (output != orig) delete output;
    }
    _connectionMutex.unlock();
}

void SampleArchiver::connect(SampleSource* source) throw()
{
    n_u::Autolock alock(_connectionMutex);

    if (getRaw()) {
        SampleSource* src = source->getRawSampleSource();
        if (src) source = src;
    }
    else {
        SampleSource* src = source->getProcessedSampleSource();
        if (src) source = src;
    }

    // on first SampleSource connection, request output connections.
    if (_connectedSources.size() == 0) {
        const list<SampleOutput*>& outputs = getOutputs();
        list<SampleOutput*>::const_iterator oi = outputs.begin();
        for ( ; oi != outputs.end(); ++oi) {
            SampleOutput* output = *oi;
            // some SampleOutputs want to know what they are getting
            output->addSourceSampleTags(source->getSampleTags());
            SampleOutputRequestThread::getInstance()->addConnectRequest(output,this,0);
        }
    }
    set<SampleOutput*>::const_iterator oi =
    	_connectedOutputs.begin();
    for ( ; oi != _connectedOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        cerr << "SampleArchiver: connecting " << output->getName() << endl;
#ifdef DEBUG
#endif
        source->addSampleClient(output);
    }
    _connectedSources.insert(source);

}
 
void SampleArchiver::disconnect(SampleSource* source) throw()
{
    n_u::Autolock alock(_connectionMutex);
    set<SampleOutput*>::const_iterator oi =
    	_connectedOutputs.begin();

    if (getRaw()) {
        SampleSource* src = source->getRawSampleSource();
        if (src) source = src;
    }
    else {
        SampleSource* src = source->getProcessedSampleSource();
        if (src) source = src;
    }

    for ( ; oi != _connectedOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        source->removeSampleClient(output);
    }
    _connectedSources.erase(source);
}
 
void SampleArchiver::connect(SampleOutput* output) throw()
{
    n_u::Logger::getInstance()->log(LOG_INFO,
        "SampleArchiver: connection from %s", output->getName().c_str());

#ifdef DEBUG
    cerr << "SampleArchiver::connnect(SampleOutput*), #sources=" <<
        _connectedSources.size() << endl;
#endif

    _connectionMutex.lock();
    _connectedOutputs.insert(output);

    set<SampleSource*>::const_iterator si = _connectedSources.begin();
    for ( ; si != _connectedSources.end(); ++si) {
        SampleSource* source = *si;
        if (getRaw()) {
            if (!output->isRaw())
                WLOG(("SampleOutput from a raw SampleArchiver is not a raw output. Use RawSampleOutputStream, rather than SampleOutputStream"));
        }
        else {
            if (output->isRaw())
                WLOG(("SampleOutput from a processed SampleArchiver is not processed output. Use SampleOutputStream, rather than RawSampleOutputStream"));
        }
        source->addSampleClient(output);
    }
    _connectionMutex.unlock();

    nidas::core::FileSet* fset = dynamic_cast<nidas::core::FileSet*>(output->getIOChannel());
    if (fset) {
        _filesetMutex.lock();
        _filesets.push_back(fset);
        _filesetMutex.unlock();
    }
}
 
void SampleArchiver::disconnect(SampleOutput* output) throw()
{
    // disconnect the output from my sources.

    _connectionMutex.lock();
    set<SampleSource*>::const_iterator si = _connectedSources.begin();
    for ( ; si != _connectedSources.end(); ++si) {
        SampleSource* source = *si;
        source->removeSampleClient(output);
    }
    _connectedOutputs.erase(output);
    _connectionMutex.unlock();

    nidas::core::FileSet* fset = dynamic_cast<nidas::core::FileSet*>(output->getIOChannel());
    if (fset) {
        _filesetMutex.lock();
        list<const nidas::core::FileSet*>::iterator fi =
            std::find(_filesets.begin(),_filesets.end(),fset);
        if (fi != _filesets.end()) _filesets.erase(fi);
        _filesetMutex.unlock();
    }

    try {
        output->finish();
    }
    catch (const n_u::IOException& ioe) {
    }
    try {
        output->close();
    }
    catch (const n_u::IOException& ioe) {
        n_u::Logger::getInstance()->log(LOG_ERR,
            "SampleArchiver: error closing %s: %s",
                output->getName().c_str(),ioe.what());
    }

    SampleOutput* orig = output->getOriginal();

    if (orig != output)
        // this will schedule output to be deleted. Don't access it after this.
        SampleOutputRequestThread::getInstance()->addDeleteRequest(output);

    // submit connection request on original output
    int delay = orig->getReconnectDelaySecs();
    if (delay < 0) return;
    SampleOutputRequestThread::getInstance()->addConnectRequest(orig,this,delay);
}

void SampleArchiver::printStatus(ostream& ostr,float deltat,int &zebra)
    throw()
{
    const char* oe[2] = {"odd","even"};

    SampleSource* source = 0;
    _connectionMutex.lock();
    if (_connectedSources.size() > 0) source = *_connectedSources.begin();
    _connectionMutex.unlock();

    const SampleStats* stats = (source ? &source->getSampleStats() : 0);

    ostr <<
        "<tr class=" << oe[zebra++%2] << "><td align=left>archive</td>";
    dsm_time_t tt = 0LL;
    if (stats) tt = stats->getLastTimeTag();
    if (tt > 0LL)
        ostr << "<td>" << n_u::UTime(tt).format(true,"%Y-%m-%d&nbsp;%H:%M:%S.%1f") << "</td>";
    else
        ostr << "<td><font color=red>Not active</font></td>";
    size_t nsamps = (stats ? stats->getNumSamples() : 0);
    float samplesps = (float)(nsamps - _nsampsLast) / deltat;

    long long nbytes = (stats ? stats->getNumBytes() : 0);
    float bytesps = (float)(nbytes - _nbytesLast) / deltat;

    _nsampsLast = nsamps;
    _nbytesLast = nbytes;

    bool warn = fabs(bytesps) < 0.0001;
    ostr <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        fixed << setprecision(1) << samplesps <<
        (warn ? "</b></font></td>" : "</td>") <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        setprecision(0) << bytesps <<
        (warn ? "</b></font></td>" : "</td>");
    ostr << "<td></td><td></td></tr>\n";

    ostringstream osstr;

    n_u::Autolock alock(_filesetMutex);

    list<const nidas::core::FileSet*>::const_iterator fi = _filesets.begin();
    for ( ; fi != _filesets.end(); ++fi) {
        const nidas::core::FileSet* fset = *fi;
        if (fset) {
            osstr <<
                "<tr class=" << oe[zebra++%2] << "><td align=left colspan=3>" <<
                fset->getCurrentName() << "</td>"; // TODO remove path info?

            long long nbytes = fset->getFileSize();
            float bytesps = (float)(nbytes - _nbytesLastByFileSet[fset]) / deltat;

            _nbytesLastByFileSet[fset] = nbytes;

            bool warn = fabs(bytesps) < 0.0001;
            osstr <<
                (warn ? "<td><font color=red><b>" : "<td>") <<
                setprecision(0) << bytesps <<
                (warn ? "</b></font></td>" : "</td>") <<
                "<td>" << setprecision(2) << nbytes / 1000000.0 << "</td>";
            int err = fset->getLastErrno();
            warn = (err != 0) && (nbytes == 0);
            osstr <<
                "<td align=left>" <<
                "status=" <<
                (warn ? "<font color=red><b>" : "") <<
                (warn ? strerror(err) : "OK") <<
                (warn ? "</b></font></td>" : "</td>");
            osstr << "</tr>\n";
        }
    }

    if ( _filesets.size() ) {
        _lastFileSetState = osstr.str();
        _lastZebra        = zebra;
    }
    else
        zebra = _lastZebra;

    ostr << _lastFileSetState;
}
