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

namespace n_u = nidas::util;

std::string SampleInputWrapper::getName() const
{
    return "SampleInputWrapper";
}

const std::list<const SampleTag*>& SampleInputWrapper::getSampleTags() const
{
    return _src->getSampleTags();
}

/*
 * Client wants samples from the process() method of the
 * given DSMSensor.
 */
void SampleInputWrapper::addProcessedSampleClient(SampleClient* clnt,
    DSMSensor* snsr)
{
    _src->addSampleClient(snsr);
    snsr->addSampleClient(clnt);
}

void SampleInputWrapper::removeProcessedSampleClient(SampleClient* clnt,
    DSMSensor* snsr)
{
    _src->removeSampleClient(snsr);
    snsr->removeSampleClient(clnt);
}

/*
 * Client wants samples from the process() method of the
 * given DSMSensor.
 */
void SampleInputWrapper::addSampleClient(SampleClient* clnt) throw()
{
    _src->addSampleClient(clnt);
}

void SampleInputWrapper::removeSampleClient(SampleClient* clnt) throw()
{
    _src->removeSampleClient(clnt);
}

string DSMSensorWrapper::getName() const
{
    return "SensorWrapper";
}

const DSMConfig* DSMSensorWrapper::getDSMConfig() const { return _snsr->getDSMConfig(); }

const list<const SampleTag*>& DSMSensorWrapper::getSampleTags() const
{
    return _snsr->getSampleTags();
}

/*
 * Client wants samples from the process() method of the
 * given DSMSensor.
 */
void DSMSensorWrapper::addProcessedSampleClient(SampleClient* clnt,
    DSMSensor* snsr)
{
    if (_snsr == snsr) {
        _snsr->addRawSampleClient(_snsr);
        _snsr->addSampleClient(clnt);
    }
}

void DSMSensorWrapper::removeProcessedSampleClient(SampleClient* clnt,
    DSMSensor* snsr)
{
    if (_snsr == snsr) {
        _snsr->removeRawSampleClient(_snsr);
        _snsr->removeSampleClient(clnt);
    }
}

/*
 * Client wants samples from the process() method of the
 * given DSMSensor.
 */
void DSMSensorWrapper::addSampleClient(SampleClient* clnt) throw()
{
    _snsr->addSampleClient(clnt);
}

void DSMSensorWrapper::removeSampleClient(SampleClient* clnt) throw()
{
    _snsr->removeSampleClient(clnt);
}

SampleInputMerger::SampleInputMerger() :
	_name("SampleInputMerger"),
	_inputSorter(_name + "InputSorter"),
	_procSampSorter(_name + "ProcSampSorter"),
	_unrecognizedSamples(0),
        _sorterLengthMsecs(250),
        _heapMax(100000000),_heapBlock(false)
{
}

SampleInputMerger::~SampleInputMerger()
{
    if (_inputSorter.isRunning()) {
        _inputSorter.finish();
	_inputSorter.interrupt();
	_inputSorter.join();
    }
    if (_procSampSorter.isRunning()) {
	_procSampSorter.finish();
	_procSampSorter.interrupt();
	_procSampSorter.join();
    }
}

void SampleInputMerger::finish() throw()
{
#ifdef DEBUG
    cerr << "SampleInputMerger::finish" << endl;
#endif
    if (_inputSorter.isRunning()) _inputSorter.finish();
    if (_procSampSorter.isRunning()) _procSampSorter.finish();
}

void SampleInputMerger::addInput(SampleInput* input)
{
    if (!_inputSorter.isRunning()) {
        _inputSorter.setLengthMsecs(getSorterLengthMsecs());
        _inputSorter.setHeapMax(getHeapMax());
        _inputSorter.setHeapBlock(getHeapBlock());
        _inputSorter.start();
    }
    input->addSampleClient(&_inputSorter);

    SampleTagIterator si = input->getSampleTagIterator();
    for ( ; si.hasNext(); ) {
        addSampleTag(si.next());
        _inputSorter.addSampleTag(si.next());
    }
}

void SampleInputMerger::removeInput(SampleInput* input)
{
    input->removeSampleClient(&_inputSorter);
}

void SampleInputMerger::addProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    _sensorMapMutex.lock();
    // samples with an Id equal to the sensor Id get forwarded to
    // the sensor
    _sensorMap[sensor->getId()] = sensor;
    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
    	_sensorsByClient.find(client);
    if (sci != _sensorsByClient.end()) sci->second.push_back(sensor);
    else {
        list<DSMSensor*> sensors;
	sensors.push_back(sensor);
	_sensorsByClient[client] = sensors;
    }
    _sensorMapMutex.unlock();

    // add sensor processed sample tags
    SampleTagIterator si = sensor->getSampleTagIterator();
    for ( ; si.hasNext(); ) {
	const SampleTag* stag = si.next();
        addSampleTag(stag);
	_procSampSorter.addSampleTag(stag);
    }

    _procSampSorter.addSampleClient(client);
    sensor->addSampleClient(&_procSampSorter);
    _inputSorter.addSampleClient(this);

    if (!_procSampSorter.isRunning()) {
        _procSampSorter.setLengthMsecs(getSorterLengthMsecs());
        _procSampSorter.setHeapMax(getHeapMax());
        _procSampSorter.setHeapBlock(getHeapBlock());
        _procSampSorter.start();
    }
}

void SampleInputMerger::removeProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    _procSampSorter.removeSampleClient(client);

    // check:
    //	are there still existing clients of procSampSorter for this sensor?
    //	simple way: remove procSampSorter as sampleClient of sensor
    //		if there are no more clients of procSampSorter
    if (_procSampSorter.getClientCount() == 0) {
	if (!sensor) {		// remove client for all sensors
	    _sensorMapMutex.lock();
	    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
		_sensorsByClient.find(client);
	    if (sci != _sensorsByClient.end()) {
		list<DSMSensor*>& sensors = sci->second;
		for (list<DSMSensor*>::iterator si = sensors.begin();
		    si != sensors.end(); ++si) {
		    sensor = *si;
		    sensor->removeSampleClient(client);
		    if (sensor->getClientCount() == 0)
			_sensorMap.erase(sensor->getId());
		}
	    }
	    _sensorMapMutex.unlock();
	}
    	else {
	    sensor->removeSampleClient(&_procSampSorter);
	    if (sensor->getClientCount() == 0) {
		_sensorMapMutex.lock();
		_sensorMap.erase(sensor->getId());
		_sensorMapMutex.unlock();
	    }
	}
	_inputSorter.removeSampleClient(this);
    }
}

/*
 * Redirect addSampleClient requests to inputSorter.
 */
void SampleInputMerger::addSampleClient(SampleClient* client) throw()
{
    _inputSorter.addSampleClient(client);
}

void SampleInputMerger::removeSampleClient(SampleClient* client) throw()
{
    _inputSorter.removeSampleClient(client);
}


void SampleInputMerger::addSampleTag(const SampleTag* tag)
{
    if (find(_sampleTags.begin(),_sampleTags.end(),tag) == _sampleTags.end())
        _sampleTags.push_back(tag);
}

bool SampleInputMerger::receive(const Sample* samp) throw()
{
    // pass sample to the appropriate sensor for distribution.
    dsm_sample_id_t sampid = samp->getId();

    _sensorMapMutex.lock();
    map<unsigned int,DSMSensor*>::const_iterator sensori
	    = _sensorMap.find(sampid);

    if (sensori != _sensorMap.end()) {
	DSMSensor* sensor = sensori->second;
	_sensorMapMutex.unlock();
	sensor->receive(samp);
	return true;
    }
    _sensorMapMutex.unlock();

    if (!(_unrecognizedSamples++ % 100)) {
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "SampleInputMerger unrecognizedSamples=%d, id=%d,%d",
                _unrecognizedSamples,
                GET_DSM_ID(samp->getId()),GET_SHORT_ID(samp->getId()));
    }

    return false;
}
