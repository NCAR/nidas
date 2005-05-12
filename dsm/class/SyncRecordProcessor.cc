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
	SampleIOProcessor(),input(0)
{
    setName("SyncRecordProcessor");
}

SyncRecordProcessor::SyncRecordProcessor(const SyncRecordProcessor& x):
	SampleIOProcessor((const SampleIOProcessor&)x),input(0)
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

void SyncRecordProcessor::connect(SampleInput* newinput)
	throw(atdUtil::IOException)
{
    input = newinput;
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	input->getName().c_str(),getName().c_str());

    const list<const DSMConfig*>& dsms =  input->getDSMConfigs();
    generator.init(dsms);

    set<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	output->setDSMConfigs(input->getDSMConfigs());
	// output->setDSMService(getDSMService());
	output->requestConnection(this);
    }

    // assert(input->isRaw());

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
 
void SyncRecordProcessor::disconnect(SampleInput* inputarg)
	throw(atdUtil::IOException)
{
    if (!input) return;
    assert(input == inputarg);

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
    set<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
        input->removeSampleClient(output);
        output->flush();
        output->close();
    }
}
 
void SyncRecordProcessor::connected(SampleOutput* output) throw()
{
    addOutput(output);
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

