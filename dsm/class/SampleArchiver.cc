/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <SampleArchiver.h>
#include <DSMConfig.h>
#include <DSMServer.h>
#include <SampleInput.h>

#include <atdUtil/Logger.h>

// #include <algo.h>

using namespace dsm;
using namespace std;

CREATOR_FUNCTION(SampleArchiver)

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

void SampleArchiver::connect(SampleInput* newinput) throw(atdUtil::IOException)
{
    input = newinput;
    SampleIOProcessor::connect(newinput);
}
 
void SampleArchiver::disconnect(SampleInput* oldinput) throw(atdUtil::IOException)
{
    if (!input) return;
    assert(input == oldinput);

    list<SampleOutput*>::iterator oi;
    for (oi = conOutputs.begin(); oi != conOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        input->removeSampleClient(output);
    }

    SampleIOProcessor::disconnect(input);
    input = 0;
}
 
void SampleArchiver::connected(SampleOutput* output) throw()
{
    assert(input);
    SampleIOProcessor::connected(output);
    input->addSampleClient(output);
}
 
void SampleArchiver::disconnected(SampleOutput* output) throw()
{
    assert(input);
    input->removeSampleClient(output);
    SampleIOProcessor::disconnected(output);
}
