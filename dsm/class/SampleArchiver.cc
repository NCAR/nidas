/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#include <SampleArchiver.h>
#include <DSMConfig.h>
#include <SampleInput.h>

#include <atdUtil/Logger.h>

// #include <algo.h>

using namespace dsm;
using namespace std;

CREATOR_ENTRY_POINT(SampleArchiver)

SampleArchiver::SampleArchiver(): SampleIOProcessor(),
	sorter(250),initialized(false)
{
    setName("SampleArchiver");
}

SampleArchiver::SampleArchiver(const SampleArchiver& x):
	SampleIOProcessor((const SampleIOProcessor&)x),
	sorter(250),initialized(false)
{
    setName("SampleArchiver");
}

SampleArchiver::~SampleArchiver()
{
}

SampleIOProcessor* SampleArchiver::clone() const {
    return new SampleArchiver(*this);
}

void SampleArchiver::connect(SampleInput* input) throw(atdUtil::IOException)
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s (%s) has connected to %s",
	input->getName().c_str(),
	(input->getDSMConfig() ? input->getDSMConfig()->getName().c_str() : ""),
	getName().c_str());


    {
	atdUtil::Synchronized autosync(initMutex);
	if (!initialized) {
	    sorter.start();
	    list<SampleOutput*>::const_iterator oi;
	    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
		SampleOutput* output = *oi;
		output->setDSMConfig(input->getDSMConfig());
		output->requestConnection(this);
	    }
	}
	initialized = true;
    }
    input->addSampleClient(&sorter);
}
 
void SampleArchiver::disconnect(SampleInput* input) throw(atdUtil::IOException)
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	input->getName().c_str(),getName().c_str());
    input->removeSampleClient(&sorter);
}
 
void SampleArchiver::connected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	output->getName().c_str(),
	getName().c_str());

    output->init();
    sorter.addSampleClient(output);
}
 
void SampleArchiver::disconnected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	output->getName().c_str(),
	getName().c_str());

    sorter.removeSampleClient(output);
    output->close();
}

