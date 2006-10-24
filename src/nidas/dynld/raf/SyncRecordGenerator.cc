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

#include <cmath>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,SyncRecordGenerator);

SyncRecordGenerator::SyncRecordGenerator():
	SampleIOProcessor(),input(0)
{
    setName("SyncRecordGenerator");
}

SyncRecordGenerator::SyncRecordGenerator(const SyncRecordGenerator& x):
	SampleIOProcessor((const SampleIOProcessor&)x),input(0)
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
    input = newinput;
    syncRecSource.connect(input);
    SampleIOProcessor::connect(input);
}
 
void SyncRecordGenerator::disconnect(SampleInput* oldinput)
	throw(n_u::IOException)
{
    if (!input) return;
    assert(input == oldinput);

    syncRecSource.disconnect(input);

    list<SampleOutput*>::const_iterator oi = getConnectedOutputs().begin();
    for ( ; oi != getConnectedOutputs().end(); ++oi) {
        SampleOutput* output = *oi;
	syncRecSource.removeSampleClient(output);
    }

    SampleIOProcessor::disconnect(input);
}
 
void SyncRecordGenerator::connected(SampleOutput* orig,
	SampleOutput* output) throw()
{
    SampleIOProcessor::connected(orig,output);
    syncRecSource.addSampleClient(output);
    output->setHeaderSource(this);
}

void SyncRecordGenerator::disconnected(SampleOutput* output) throw()
{
    syncRecSource.removeSampleClient(output);
    SampleIOProcessor::disconnected(output);
    output->setHeaderSource(0);
}

void SyncRecordGenerator::sendHeader(dsm_time_t thead,SampleOutput* output)
	throw(n_u::IOException)
{
    HeaderSource::sendDefaultHeader(output);
    // syncRecSource sends a header sample to the stream
    syncRecSource.sendHeader(thead);
}

