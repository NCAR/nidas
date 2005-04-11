/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

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

SampleArchiver::SampleArchiver(): SampleIOProcessor()
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

    inputListMutex.lock();
    int ninputs = inputs.size();
    inputs.push_back(input);
    inputListMutex.unlock();

    list<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	output->setDSMConfig(input->getDSMConfig());
	if (ninputs == 0) output->requestConnection(this);
	else input->addSampleClient(output);
    }
}
 
void SampleArchiver::disconnect(SampleInput* input) throw(atdUtil::IOException)
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	input->getName().c_str(),getName().c_str());

    list<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
	input->removeSampleClient(output);
    }

    atdUtil::Synchronized autolock(inputListMutex);
    list<SampleInput*>::iterator ii = find(inputs.begin(),inputs.end(),input);
    if (ii != inputs.end()) inputs.erase(ii);
}
 
void SampleArchiver::connected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	output->getName().c_str(),
	getName().c_str());

    output->init();

    atdUtil::Synchronized autolock(inputListMutex);
    list<SampleInput*>::const_iterator ii;
    for (ii = inputs.begin(); ii != inputs.end(); ++ii) {
        SampleInput* input = *ii;
	input->addSampleClient(output);
    }
}
 
void SampleArchiver::disconnected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	output->getName().c_str(),
	getName().c_str());

    atdUtil::Synchronized autolock(inputListMutex);
    list<SampleInput*>::const_iterator ii;
    for (ii = inputs.begin(); ii != inputs.end(); ++ii) {
        SampleInput* input = *ii;
	input->removeSampleClient(output);
    }
    output->close();
}

