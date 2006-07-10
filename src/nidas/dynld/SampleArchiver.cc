/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/dynld/SampleArchiver.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/SampleInput.h>

#include <nidas/util/Logger.h>

// #include <algo.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(SampleArchiver)

SampleArchiver::SampleArchiver(): SampleIOProcessor(),input(0)
{
    setName("SampleArchiver");
}

SampleArchiver::SampleArchiver(const SampleArchiver& x):
	SampleIOProcessor((const SampleIOProcessor&)x),input(0)
{
    setName("SampleArchiver");
}

SampleArchiver::~SampleArchiver()
{
}

SampleArchiver* SampleArchiver::clone() const {
    return new SampleArchiver(*this);
}

void SampleArchiver::connect(SampleInput* newinput) throw(n_u::IOException)
{
    input = newinput;
    SampleTagIterator ti = input->getSampleTagIterator();
    for ( ; ti.hasNext(); ) {
	const SampleTag* stag = ti.next();
	addSampleTag(new SampleTag(*stag));
    }
    SampleIOProcessor::connect(input);
}
 
void SampleArchiver::disconnect(SampleInput* oldinput) throw(n_u::IOException)
{
    if (!input) return;
    assert(input == oldinput);

    const list<SampleOutput*>& cnctdOutputs = getConnectedOutputs();
    list<SampleOutput*>::const_iterator oi =
    	cnctdOutputs.begin();
    for ( ; oi != cnctdOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        input->removeSampleClient(output);
    }

    SampleIOProcessor::disconnect(input);
    input = 0;
}
 
void SampleArchiver::connected(SampleOutput* orig,SampleOutput* output) throw()
{
    assert(input);
    SampleIOProcessor::connected(orig,output);
    input->addSampleClient(output);
}
 
void SampleArchiver::disconnected(SampleOutput* output) throw()
{
    if (input) input->removeSampleClient(output);
    SampleIOProcessor::disconnected(output);
}
