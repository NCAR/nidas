/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#include <SyncRecordProcessor.h>
#include <DSMSerialSensor.h>
#include <DSMArincSensor.h>
#include <Aircraft.h>
#include <irigclock.h>

#include <math.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SyncRecordProcessor);

SyncRecordProcessor::SyncRecordProcessor():
	sorter(250),initialized(false)
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
    assert(false);
    // return new SyncRecordProcessor();
    return 0;
}

void SyncRecordProcessor::connect(SampleInput* input)
	throw(atdUtil::IOException)
{
    cerr << "SyncRecordProcessor:: connect: " << getName() <<
    	" input=" << input->getName() << endl;

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
	cerr << "SyncRecordProcessor::connect " << input->getName() <<
	    " connecting to " << sensor->getName() << endl;
	sensor->addSampleClient(&sorter);
	input->addSensor(sensor);
    }
}
 
void SyncRecordProcessor::disconnect(SampleInput* input)
	throw(atdUtil::IOException)
{
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
    output->init();
    cerr << "SyncRecordProcessor:: connected: " << getName() <<
    	" output=" << output->getName() << endl;
    generator.addSampleClient(output);

}

void SyncRecordProcessor::disconnected(SampleOutput* output) throw()
{
    cerr << "SyncRecordProcessor:: disconnected: " << getName() <<
    	" output=" << output->getName() << endl;
    generator.removeSampleClient(output);
}

