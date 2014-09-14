// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
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

#include <nidas/dynld/raf/SyncRecordGenerator.h>
#include <nidas/dynld/raf/DSMArincSensor.h>

#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/core/Version.h>

#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#include <iomanip>
#include <cmath>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,SyncRecordGenerator);

SyncRecordGenerator::SyncRecordGenerator():
    SampleIOProcessor(true),
    _connectionMutex(),_connectedSources(),_connectedOutputs(),
    _syncRecSource(),
    _numInputSampsLast(0),_numOutputSampsLast(0),
    _numInputBytesLast(0),_numOutputBytesLast(0)
{
    setName("SyncRecordGenerator");
}

SyncRecordGenerator::~SyncRecordGenerator()
{
   _connectionMutex.lock();
    set<SampleOutput*>::const_iterator oi = _connectedOutputs.begin();
    for ( ; oi != _connectedOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        _syncRecSource.removeSampleClient(output);
        output->flush();
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

void SyncRecordGenerator::connect(SampleSource* source) throw()
{
    n_u::Autolock alock(_connectionMutex);
    // on first SampleSource connection, request output connections.
    // We could add the outputs to the SyncRecordSource and have
    // it request the connections, but this works too.

    // GJG: It looks like the outputs are passed the SampleTags from the
    // SyncRecordSource, but as far as I can tell SyncRecordSource does not
    // have any SampleTags until after the source is connected to it in the
    // connect() call at the end of this method...  Apparently it's not
    // hurting anything, other than my understanding of what's going on. :)

    if (_connectedSources.size() == 0) {
        const list<SampleOutput*>& outputs = getOutputs();
        list<SampleOutput*>::const_iterator oi = outputs.begin();
        for ( ; oi != outputs.end(); ++oi) {
            SampleOutput* output = *oi;
            // some SampleOutputs want to know what they are getting
            output->addSourceSampleTags(_syncRecSource.getSampleTags());
            SampleOutputRequestThread::getInstance()->addConnectRequest(output,this,0);
        }
    }
    _connectedSources.insert(source);

    _syncRecSource.connect(source);
}
 
void SyncRecordGenerator::disconnect(SampleSource* source) throw()
{
    n_u::Autolock alock(_connectionMutex);
    _syncRecSource.disconnect(source);
    _syncRecSource.flush();
    _connectedSources.erase(source);
}
 
void SyncRecordGenerator::connect(SampleOutput* output) throw()
{

    _connectionMutex.lock();
    _syncRecSource.addSampleClient(output);
    output->setHeaderSource(this);
    _connectedOutputs.insert(output);
    _connectionMutex.unlock();
}

void SyncRecordGenerator::disconnect(SampleOutput* output) throw()
{

   _connectionMutex.lock();
    output->setHeaderSource(0);
    _syncRecSource.removeSampleClient(output);
    _connectedOutputs.erase(output);
    _connectionMutex.unlock();

    output->flush();
    try {
        output->close();
    }
    catch (const n_u::IOException& ioe) {
        n_u::Logger::getInstance()->log(LOG_ERR,
            "DSMEngine: error closing %s: %s",
                output->getName().c_str(),ioe.what());
    }

    SampleOutput* orig = output->getOriginal();

    if (orig != output)
        SampleOutputRequestThread::getInstance()->addDeleteRequest(output);

    int delay = orig->getReconnectDelaySecs();
    if (delay < 0) return;

    // submit connection request on original output
    SampleOutputRequestThread::getInstance()->addConnectRequest(orig,this,delay);
}

void SyncRecordGenerator::sendHeader(dsm_time_t, SampleOutput* output)
	throw(n_u::IOException)
{
    HeaderSource::sendDefaultHeader(output);
    // SyncRecordSource now sends a header sample when the first sample is
    // receive()d, prior to sending the sync samples, since technically
    // that was never part of the real header required by a SampleOutput.
    // This way every SampleClient gets the sync header sample, not just
    // the SampleOutput instances.
    //
    //    _syncRecSource.sendHeader(thead);
}

void SyncRecordGenerator::printStatus(ostream& ostr,float deltat,int &zebra)
    throw()
{
    const char* oe[2] = {"odd","even"};

    SampleSource* source = 0;
    _connectionMutex.lock();
    if (_connectedSources.size() > 0) source = *_connectedSources.begin();
    _connectionMutex.unlock();

    const SampleStats* stats = (source ? &source->getSampleStats() : 0);

    ostr <<
        "<tr class=" << oe[zebra++%2] << "><td align=left>sync_gen source</td>";

    dsm_time_t tt = 0LL;
    if (stats) tt = stats->getLastTimeTag();
    if (tt > 0LL)
        ostr << "<td>" << n_u::UTime(tt).format(true,"%Y-%m-%d&nbsp;%H:%M:%S.%1f") <<
            "</td>";
    else
        ostr << "<td><font color=red>>Not active</font></td>";
    size_t nsamps = (stats ? stats->getNumSamples() : 0);
    float samplesps = (float)(nsamps - _numInputSampsLast) / deltat;

    long long nbytes = (stats ? stats->getNumBytes() : 0);
    float bytesps = (float)(nbytes - _numInputBytesLast) / deltat;

    _numInputSampsLast = nsamps;
    _numInputBytesLast = nbytes;

    bool warn = fabs(bytesps) < 0.0001;
    ostr <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        fixed << setprecision(1) << samplesps <<
        (warn ? "</b></font></td>" : "</td>") <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        setprecision(0) << bytesps <<
        (warn ? "</b></font></td>" : "</td>");
    ostr << "<td></td><td></td></tr>\n";

#ifdef SUPPORT_THIS
    ostr <<
        "<tr class=" << oe[zebra++%2] << "><td align=left>sync_gen output</td>";
    tt = 0LL;
    if (_output) tt = _output->getLastReceivedTimeTag();
    if (tt > 0LL)
        ostr << "<td>" << n_u::UTime(tt).format(true,"%Y-%m-%d&nbsp;%H:%M:%S.%1f") <<
            "</td>";
    else
        ostr << "<td><font color=red>>Not active</font></td>";
    nsamps = (_output ? _output->getNumReceivedSamples() : 0);
    samplesps = (float)(nsamps - _numOutputSampsLast) / deltat;

    nbytes = (_output ? _output->getNumReceivedBytes() : 0);
    bytesps = (float)(nbytes - _numOutputBytesLast) / deltat;

    _numOutputSampsLast = nsamps;
    _numOutputBytesLast = nbytes;

    warn = fabs(bytesps) < 0.0001;
    ostr <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        fixed << setprecision(1) << samplesps <<
        (warn ? "</b></font></td>" : "</td>") <<
        (warn ? "<td><font color=red><b>" : "<td>") <<
        setprecision(0) << bytesps <<
        (warn ? "</b></font></td>" : "</td>");
    ostr << "<td></td><td></td></tr>\n";
#endif
}
