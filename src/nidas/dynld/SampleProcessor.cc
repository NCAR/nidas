
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
#include <nidas/core/Project.h>

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(SampleProcessor)

SampleProcessor::SampleProcessor():
	SampleIOProcessor(),_input(0)
{
    setName("SampleProcessor");
}

SampleProcessor::~SampleProcessor()
{
}

void SampleProcessor::connect(SampleInput* input) throw()
{
    ILOG(("%s connect to SampleProcessor",input->getName().c_str()) << " _ input=" << _input);
    if (!_input) {
        const list<SampleOutput*>& outputs = getOutputs();
        list<SampleOutput*>::const_iterator oi = outputs.begin();
        for ( ; oi != outputs.end(); ++oi) {
            SampleOutput* output = *oi;
            ILOG(("SampleProcessor requesting Connection of %s",output->getName().c_str()));
            output->requestConnection(this);
        }
    }
    _connectedInputs.push_back(input);
    _input = input;
}

void SampleProcessor::disconnect(SampleInput* input)
        throw()
{
    if (!_input) return;
    // assert(_input == input);

    Project* project = Project::getInstance();
    const set<SampleOutput*>& outputs = getConnectedOutputs();
    set<SampleOutput*>::const_iterator oi = outputs.begin();
    for ( ; oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
        SensorIterator si = project->getSensorIterator();
        for ( ; si.hasNext(); ) {
            DSMSensor* sensor = si.next();
            input->removeProcessedSampleClient(output,sensor);
        }
    }
    SampleIOProcessor::disconnect(input);
}

void SampleProcessor::connect(SampleOutput* orig, SampleOutput* output) throw()
{
    // cerr << "SampleProcessor::connect(SampleOutput*), input.size==" << _connectedInputs.size() << endl;
    // cerr << "SampleProcessor::connect(SampleOutput*), orig=" << orig->getName() << " output=" << output->getName() << endl;
    // if (!_input) return;
    Project* project = Project::getInstance();
    SensorIterator si = project->getSensorIterator();
    for ( ; si.hasNext(); ) {
        DSMSensor* sensor = si.next();
        // cerr << "sensor=" << sensor->getDeviceName() << endl;
        list<SampleInput*>::const_iterator ii = _connectedInputs.begin();
        for ( ; ii != _connectedInputs.end(); ++ii) {
            SampleInput* input = *ii;
            input->addProcessedSampleClient(output,sensor);
        }
    }
    SampleIOProcessor::connect(orig,output);
}
void SampleProcessor::disconnect(SampleOutput* output) throw()
{
    if (!_input) return;
    Project* project = Project::getInstance();
    SensorIterator si = project->getSensorIterator();
    for ( ; si.hasNext(); ) {
        DSMSensor* sensor = si.next();
        list<SampleInput*>::const_iterator ii = _connectedInputs.begin();
        for ( ; ii != _connectedInputs.end(); ++ii) {
            SampleInput* input = *ii;
            input->removeProcessedSampleClient(output,sensor);
        }
    }
    SampleIOProcessor::disconnect(output);
}
