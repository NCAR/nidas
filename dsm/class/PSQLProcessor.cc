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

CREATOR_FUNCTION(PSQLProcessor)

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

PSQLProcessor* PSQLProcessor::clone() const {
    return new PSQLProcessor(*this);
}

void PSQLProcessor::connect(SampleInput* newinput) throw(atdUtil::IOException)
{
    input = newinput;

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

                if (!tag->isProcessed()) continue;

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
            input->addProcessedSampleClient(&averager,sensor);
        }
    }
    SampleIOProcessor::connect(input);

}
 
void PSQLProcessor::disconnect(SampleInput* oldinput) throw(atdUtil::IOException)
{
    if (!input) return;
    assert(input == oldinput);

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
    SampleIOProcessor::disconnect(input);
    input = 0;
}
 
void PSQLProcessor::connected(SampleOutput* output) throw()
{
    PSQLSampleOutput* psqlOutput =
	    dynamic_cast<PSQLSampleOutput*>(output);
    if (psqlOutput) psqlOutput->addSampleTag(averager.getSampleTag());
    else atdUtil::Logger::getInstance()->log(LOG_ERR,
	"%s is not a PSQLSampleOutput", output->getName().c_str());

    SampleIOProcessor::connected(output);
    averager.addSampleClient(output);
}
 
void PSQLProcessor::disconnected(SampleOutput* output) throw()
{
    averager.removeSampleClient(output);
    SampleIOProcessor::disconnected(output);
}

