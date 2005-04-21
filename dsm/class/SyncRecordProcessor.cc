/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <SyncRecordProcessor.h>
#include <SampleFileHeader.h>
#include <DSMSerialSensor.h>
#include <DSMArincSensor.h>
#include <Aircraft.h>
#include <Version.h>

#include <atdUtil/Logger.h>

#include <math.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SyncRecordProcessor);

SyncRecordProcessor::SyncRecordProcessor():
	SampleIOProcessor(),sorter(250),initialized(false)
{
    setName("SyncRecordProcessor");
}

SyncRecordProcessor::~SyncRecordProcessor()
{
    if (initialized) {
	sorter.interrupt();
	sorter.join();
    }
}

SampleIOProcessor* SyncRecordProcessor::clone() const
{
    // this shouldn't be cloned
    assert(false);
    // return new SyncRecordProcessor();
    return 0;
}

void SyncRecordProcessor::connect(SampleInput* input)
	throw(atdUtil::IOException)
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	input->getName().c_str(),getName().c_str());

    {
	atdUtil::Synchronized autosync(initMutex);
	if (!initialized) {
	    sorter.start();
	    const list<DSMConfig*>& dsms = 
		getDSMService()->getDSMServer()->getAircraft()->getDSMConfigs();
	    generator.init(dsms);
	    sorter.addSampleClient(&generator);

	    list<SampleOutput*>::const_iterator oi;
	    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
		SampleOutput* output = *oi;
		output->setDSMService(getDSMService());
		output->requestConnection(this);
	    }
	    initialized = true;
	}
    }

    assert(input->isRaw());
    const list<DSMSensor*>& sensors = input->getDSMConfig()->getSensors();
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
#ifdef DEBUG
	cerr << "SyncRecordProcessor::connect, input=" <<
		input->getName() << " sensor=" <<
		    sensor->getName() << endl;
#endif
	sensor->addSampleClient(&sorter);
	input->addSensor(sensor);
    }
}
 
void SyncRecordProcessor::disconnect(SampleInput* input)
	throw(atdUtil::IOException)
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	input->getName().c_str(),getName().c_str());

    if (input->getDSMConfig()) {
	const list<DSMSensor*>& sensors = input->getDSMConfig()->getSensors();
	list<DSMSensor*>::const_iterator si;
	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    sensor->removeSampleClient(&sorter);
	}
    }
}
 
void SyncRecordProcessor::connected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	output->getName().c_str(),getName().c_str());

    output->init();
    generator.addSampleClient(output);

}

void SyncRecordProcessor::disconnected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	output->getName().c_str(),getName().c_str());

    generator.removeSampleClient(output);
}

void SyncRecordProcessor::newFileCallback(dsm_time_t thead,IOStream* iostream)
	throw(atdUtil::IOException)
{
    SampleFileHeader header;
    header.setArchiveVersion(Version::getArchiveVersion());
    header.setSoftwareVersion(Version::getSoftwareVersion());
    header.setProjectName(Project::getInstance()->getName());
    header.setXMLName(Project::getInstance()->getXMLName());
    header.setXMLVersion(Project::getInstance()->getVersion());
    header.write(iostream);

    generator.sendHeader(thead);
}

