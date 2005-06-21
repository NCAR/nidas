/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <PSQLProcessor.h>
#include <PSQLSampleOutput.h>
#include <DSMConfig.h>

#include <atdUtil/Logger.h>

// #include <algo.h>

using namespace dsm;
using namespace std;

CREATOR_ENTRY_POINT(PSQLProcessor)

PSQLProcessor::PSQLProcessor(): SampleIOProcessor(),input(0)
{
    setName("PSQLProcessor");
    averager.setAveragePeriod(MSECS_PER_SEC);
}

PSQLProcessor::PSQLProcessor(const PSQLProcessor& x):
	SampleIOProcessor((const SampleIOProcessor&)x),input(0),
	averager(x.averager)
{
    setName("PSQLProcessor");
}

PSQLProcessor::~PSQLProcessor()
{
}

SampleIOProcessor* PSQLProcessor::clone() const {
    return new PSQLProcessor(*this);
}

void PSQLProcessor::connect(SampleInput* newinput) throw(atdUtil::IOException)
{
    input = newinput;
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	input->getName().c_str(), getName().c_str());

    const list<const DSMConfig*>& dsms = input->getDSMConfigs();
    list<const DSMConfig*>::const_iterator di;
    for (di = dsms.begin(); di != dsms.end(); ++di) {
        const DSMConfig* dsm = *di;

	const list<DSMSensor*>& sensors = dsm->getSensors();
	list<DSMSensor*>::const_iterator si;
	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    const vector<const SampleTag*>& tags = sensor->getSampleTags();

	    vector<const SampleTag*>::const_iterator ti;
	    for (ti = tags.begin(); ti != tags.end(); ++ti) {
	    	const SampleTag* tag = *ti;             

		const vector<const Variable*>& vars = tag->getVariables();
		vector<const Variable*>::const_iterator vi;
		for (vi = vars.begin(); vi != vars.end(); ++vi) {
		    const Variable* var = *vi;
		    averager.addVariable(var);
		}
	    }
	}
    }

    averager.init();

    set<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	output->setDSMConfigs(dsms);
	output->requestConnection(this);
    }


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
            input->addProcessedSampleClient(&averager,sensor);
        }
    }
}
 
void PSQLProcessor::disconnect(SampleInput* inputarg) throw(atdUtil::IOException)
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
            input->removeProcessedSampleClient(&averager,sensor);
        }
    }
    averager.flush();

    set<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	input->removeSampleClient(output);
	output->close();
    }
    input = 0;
}
 
void PSQLProcessor::connected(SampleOutput* output) throw()
{
    addOutput(output);
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s, #outputs=%d",
	output->getName().c_str(),
	getName().c_str(),
	outputs.size());
    try {
	PSQLSampleOutput* psqlOutput =
		dynamic_cast<PSQLSampleOutput*>(output);
	if (psqlOutput) psqlOutput->addSampleTag(averager.getSampleTag());
	else atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s is not a PSQLSampleOutput", output->getName().c_str());
	output->init();
    }
    catch( const atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: error: %s",
	    output->getName().c_str(),ioe.what());
	disconnected(output);
	return;
    }
    averager.addSampleClient(output);
}
 
void PSQLProcessor::disconnected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	output->getName().c_str(),
	getName().c_str());
    averager.removeSampleClient(output);
    output->close();
}

