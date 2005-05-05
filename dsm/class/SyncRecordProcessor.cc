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
	SampleIOProcessor(),initialized(false)
{
    setName("SyncRecordProcessor");
}

SyncRecordProcessor::~SyncRecordProcessor()
{
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
	    const list<const DSMConfig*>& dsms =  input->getDSMConfigs();
	    generator.init(dsms);

	    list<SampleOutput*>::const_iterator oi;
	    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
		SampleOutput* output = *oi;
		output->setDSMConfigs(input->getDSMConfigs());
		// output->setDSMService(getDSMService());
		output->requestConnection(this);
	    }
	    initialized = true;
	}
    }

    // assert(input->isRaw());

    const list<const DSMConfig*>& dsms = input->getDSMConfigs();
    list<const DSMConfig*>::const_iterator di;
    for (di = dsms.begin(); di != dsms.end(); ++di) {
        const DSMConfig* dsm = *di;

	const list<DSMSensor*>& sensors = dsm->getSensors();
	list<DSMSensor*>::const_iterator si;
	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    DSMSensor* sensor = *si;
#ifdef DEBUG
	    cerr << "SyncRecordProcessor::connect, input=" <<
		    input->getName() << " sensor=" <<
			sensor->getName() << endl;
#endif
	    sensor->init();
	    input->addProcessedSampleClient(&generator,sensor);
	}
    }
}
 
void SyncRecordProcessor::disconnect(SampleInput* input)
	throw(atdUtil::IOException)
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	input->getName().c_str(),getName().c_str());

    const list<const DSMConfig*>& dsms = input->getDSMConfigs();
    list<const DSMConfig*>::const_iterator di;
    for (di = dsms.begin(); di != dsms.end(); ++di) {
        const DSMConfig* dsm = *di;

	const list<DSMSensor*>& sensors = dsm->getSensors();
	list<DSMSensor*>::const_iterator si;
	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    input->removeProcessedSampleClient(&generator,sensor);
	}
    }
    generator.flush();
    list<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
        input->removeSampleClient(output);
        output->flush();
        output->close();
    }
}
 
void SyncRecordProcessor::connected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	output->getName().c_str(),getName().c_str());

    try {
	output->init();
    }
    catch(const atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: %error: %s",
	    output->getName().c_str(),ioe.what());
	return;
    }
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

