
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-04-23 12:23:02 -0600 (Mon, 23 Apr 2007) $

    $LastChangedRevision: 3841 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/SampleProcessor.cc $
 ********************************************************************

*/

#include <nidas/dynld/SampleProcessor.h>
#include <nidas/core/SampleOutputRequestThread.h>
#include <nidas/core/Project.h>

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(SampleProcessor)

SampleProcessor::SampleProcessor():
	SampleIOProcessor(false)
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
        try {
            output->finish();
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

   try {
        output->finish();
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
    SampleOutputRequestThread::getInstance()->addConnectRequest(orig,this,10);
}
