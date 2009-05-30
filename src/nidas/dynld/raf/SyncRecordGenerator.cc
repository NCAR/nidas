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
// #include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/dynld/raf/DSMArincSensor.h>
#include <nidas/core/Version.h>

#include <nidas/util/Logger.h>

#include <iomanip>
#include <cmath>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,SyncRecordGenerator);

SyncRecordGenerator::SyncRecordGenerator():
    SampleIOProcessor(),_input(0),_output(0),
    _numInputSampsLast(0),_numOutputSampsLast(0),
    _numInputBytesLast(0),_numOutputBytesLast(0)
{
    setName("SyncRecordGenerator");
}

SyncRecordGenerator::SyncRecordGenerator(const SyncRecordGenerator& x):
    SampleIOProcessor((const SampleIOProcessor&)x),_input(0),_output(0),
    _numInputSampsLast(0),_numOutputSampsLast(0),
    _numInputBytesLast(0),_numOutputBytesLast(0)
{
    setName("SyncRecordGenerator");
}

SyncRecordGenerator::~SyncRecordGenerator()
{
}

SyncRecordGenerator* SyncRecordGenerator::clone() const
{
    // this shouldn't be cloned
    assert(false);
    // return new SyncRecordGenerator();
    return 0;
}

void SyncRecordGenerator::connect(SampleInput* newinput)
	throw(n_u::IOException)
{
    _statusMutex.lock();
    _input = newinput;
    _statusMutex.unlock();
    _syncRecSource.connect(_input);
    SampleIOProcessor::connect(_input);
}
 
void SyncRecordGenerator::disconnect(SampleInput* oldinput) throw()
{
    if (!_input) return;
    assert(_input == oldinput);

    _syncRecSource.disconnect(_input);

    const set<SampleOutput*>& tmpputs = getConnectedOutputs();
    set<SampleOutput*>::const_iterator oi = tmpputs.begin();
    for ( ; oi != tmpputs.end(); ++oi) {
        SampleOutput* output = *oi;
	_syncRecSource.removeSampleClient(output);
    }

    SampleIOProcessor::disconnect(_input);
    _statusMutex.lock();
    _input = 0;
    _statusMutex.unlock();
}
 
void SyncRecordGenerator::connected(SampleOutput* orig,
	SampleOutput* output) throw()
{
    SampleIOProcessor::connected(orig,output);
    _syncRecSource.addSampleClient(output);
    output->setHeaderSource(this);

    _statusMutex.lock();
    _output = output;
    _statusMutex.unlock();
}

void SyncRecordGenerator::disconnected(SampleOutput* output) throw()
{
    _syncRecSource.removeSampleClient(output);
    SampleIOProcessor::disconnected(output);
    output->setHeaderSource(0);
    _statusMutex.lock();
    _output = 0;
    _statusMutex.unlock();
}

void SyncRecordGenerator::sendHeader(dsm_time_t thead,SampleOutput* output)
	throw(n_u::IOException)
{
    HeaderSource::sendDefaultHeader(output);
    // syncRecSource sends a header sample to the stream
    _syncRecSource.sendHeader(thead);
}

void SyncRecordGenerator::printStatus(ostream& ostr,float deltat,int &zebra)
    throw()
{
    const char* oe[2] = {"odd","even"};

    n_u::Autolock statusLock(_statusMutex);

    ostr <<
        "<tr class=" << oe[zebra++%2] << "><td align=left>sync_gen input</td>";

    dsm_time_t tt = 0LL;
    if (_input) tt = _input->getLastDistributedTimeTag();
    if (tt > 0LL)
        ostr << "<td>" << n_u::UTime(tt).format(true,"%Y-%m-%d&nbsp;%H:%M:%S.%1f") <<
            "</td>";
    else
        ostr << "<td><font color=red>>Not active</font></td>";
    size_t nsamps = (_input ? _input->getNumDistributedSamples() : 0);
    float samplesps = (float)(nsamps - _numInputSampsLast) / deltat;

    long long nbytes = (_input ? _input->getNumDistributedBytes() : 0);
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
}
