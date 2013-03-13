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

#include <nidas/dynld/SampleProcessor.h>
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/Project.h>

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(SampleProcessor)

SampleProcessor::SampleProcessor():
    SampleIOProcessor(false),_connectionMutex(),_connectedSources(),
    _connectedOutputs()
{
    setName("SampleProcessor");
}

SampleProcessor::~SampleProcessor()
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

void SampleProcessor::connect(SampleSource* source) throw()
{
    source = source->getProcessedSampleSource();

    n_u::Autolock alock(_connectionMutex);

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
        source->addSampleClient(output);
    }
    _connectedSources.insert(source);
}

void SampleProcessor::disconnect(SampleSource* source)
        throw()
{
    source = source->getProcessedSampleSource();
    n_u::Autolock alock(_connectionMutex);
    set<SampleOutput*>::const_iterator oi =
        _connectedOutputs.begin();
    for ( ; oi != _connectedOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        source->removeSampleClient(output);
    }
    _connectedSources.erase(source);
}

void SampleProcessor::connect(SampleOutput* output) throw()
{
    n_u::Autolock alock(_connectionMutex);

    set<SampleSource*>::const_iterator si = _connectedSources.begin();
    for ( ; si != _connectedSources.end(); ++si) {
        SampleSource* source = *si;
        source->addSampleClient(output);
    }
    _connectedOutputs.insert(output);
}
void SampleProcessor::disconnect(SampleOutput* output) throw()
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

    // submit connection request on original output
    int delay = orig->getReconnectDelaySecs();
    if (delay < 0) return;
    SampleOutputRequestThread::getInstance()->addConnectRequest(orig,this,delay);
}

void SampleProcessor::flush() throw()
{
    _connectionMutex.lock();
    set<SampleOutput*>::const_iterator oi = _connectedOutputs.begin();
    for ( ; oi != _connectedOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        output->flush();
    }
    _connectionMutex.unlock();
}
