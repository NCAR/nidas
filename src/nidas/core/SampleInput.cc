/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/Project.h>
#include <nidas/core/SampleInput.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/DSMService.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

SampleInputMerger::SampleInputMerger() :
	name("SampleInputMerger"),
	inputSorter(name + "InputSorter"),
	procSampSorter(name + "ProcSampSorter"),
	unrecognizedSamples(0)
{
    inputSorter.setLengthMsecs(1500);
    procSampSorter.setLengthMsecs(100);
}

SampleInputMerger::~SampleInputMerger()
{
    if (inputSorter.isRunning()) {
	inputSorter.interrupt();
	inputSorter.join();
    }
    if (procSampSorter.isRunning()) {
	procSampSorter.interrupt();
	procSampSorter.join();
    }
}

void SampleInputMerger::addInput(SampleInput* input)
{
    if (!inputSorter.isRunning()) inputSorter.start();
#ifdef DEBUG
    cerr << "SampleInputMerger: " << input->getName() << 
    	" addSampleClient, &inputSorter=" << &inputSorter << endl;
#endif
    input->addSampleClient(&inputSorter);

    SampleTagIterator si = input->getSampleTagIterator();
    for ( ; si.hasNext(); ) {
        addSampleTag(si.next());
        inputSorter.addSampleTag(si.next());
    }
}

void SampleInputMerger::removeInput(SampleInput* input)
{
    input->removeSampleClient(&inputSorter);
}

void SampleInputMerger::addProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    sensorMapMutex.lock();
    // samples with an Id equal to the sensor Id get forwarded to
    // the sensor
    sensorMap[sensor->getId()] = sensor;
    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
    	sensorsByClient.find(client);
    if (sci != sensorsByClient.end()) sci->second.push_back(sensor);
    else {
        list<DSMSensor*> sensors;
	sensors.push_back(sensor);
	sensorsByClient[client] = sensors;
    }
    sensorMapMutex.unlock();

    // add sensor processed sample tags
    SampleTagIterator si = sensor->getSampleTagIterator();
    for ( ; si.hasNext(); ) {
	const SampleTag* stag = si.next();
        addSampleTag(stag);
	procSampSorter.addSampleTag(stag);
    }

    procSampSorter.addSampleClient(client);
    sensor->addSampleClient(&procSampSorter);
    inputSorter.addSampleClient(this);

    if (!procSampSorter.isRunning()) procSampSorter.start();
}

void SampleInputMerger::removeProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    procSampSorter.removeSampleClient(client);

    // check:
    //	are there still existing clients of procSampSorter for this sensor?
    //	simple way: remove procSampSorter as sampleClient of sensor
    //		if there are no more clients of procSampSorter
    if (procSampSorter.getClientCount() == 0) {
	if (!sensor) {		// remove client for all sensors
	    sensorMapMutex.lock();
	    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
		sensorsByClient.find(client);
	    if (sci != sensorsByClient.end()) {
		list<DSMSensor*>& sensors = sci->second;
		for (list<DSMSensor*>::iterator si = sensors.begin();
		    si != sensors.end(); ++si) {
		    sensor = *si;
		    sensor->removeSampleClient(client);
		    if (sensor->getClientCount() == 0)
			sensorMap.erase(sensor->getId());
		}
	    }
	    sensorMapMutex.unlock();
	}
    	else {
	    sensor->removeSampleClient(&procSampSorter);
	    if (sensor->getClientCount() == 0) {
		sensorMapMutex.lock();
		sensorMap.erase(sensor->getId());
		sensorMapMutex.unlock();
	    }
	}
	inputSorter.removeSampleClient(this);
    }
}

/*
 * Redirect addSampleClient requests to inputSorter.
 */
void SampleInputMerger::addSampleClient(SampleClient* client) throw()
{
    inputSorter.addSampleClient(client);
}

void SampleInputMerger::removeSampleClient(SampleClient* client) throw()
{
    inputSorter.removeSampleClient(client);
}


void SampleInputMerger::addSampleTag(const SampleTag* tag)
{
    if (find(sampleTags.begin(),sampleTags.end(),tag) == sampleTags.end())
        sampleTags.push_back(tag);
}

bool SampleInputMerger::receive(const Sample* samp) throw()
{
    // pass sample to the appropriate sensor for distribution.
    dsm_sample_id_t sampid = samp->getId();

    sensorMapMutex.lock();
    map<unsigned long,DSMSensor*>::const_iterator sensori
	    = sensorMap.find(sampid);

    if (sensori != sensorMap.end()) {
	DSMSensor* sensor = sensori->second;
	sensorMapMutex.unlock();
	sensor->receive(samp);
	return true;
    }
    sensorMapMutex.unlock();

    if (!(unrecognizedSamples++ % 100)) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "SampleInputStream unrecognizedSamples=%d, id=%d,%d",
                unrecognizedSamples,
                GET_DSM_ID(samp->getId()),GET_SHORT_ID(samp->getId()));
    }

    return false;
}
