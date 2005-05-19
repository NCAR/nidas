/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <SampleArchiver.h>
#include <SampleFileHeader.h>
#include <Project.h>
#include <DSMConfig.h>
#include <DSMServer.h>
#include <SampleInput.h>
#include <Version.h>

#include <atdUtil/Logger.h>

// #include <algo.h>

using namespace dsm;
using namespace std;

CREATOR_ENTRY_POINT(SampleArchiver)

SampleArchiver::SampleArchiver(): SampleIOProcessor(),input(0)
{
    setName("SampleArchiver");
}

SampleArchiver::SampleArchiver(const SampleArchiver& x):
	SampleIOProcessor((const SampleIOProcessor&)x),input(0)
{
    setName("SampleArchiver");
}

SampleArchiver::~SampleArchiver()
{
}

SampleIOProcessor* SampleArchiver::clone() const {
    return new SampleArchiver(*this);
}

void SampleArchiver::connect(SampleInput* inputarg) throw(atdUtil::IOException)
{
    input = inputarg;
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	input->getName().c_str(), getName().c_str());

    set<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	output->setDSMConfigs(input->getDSMConfigs());
	output->requestConnection(this);
    }
}
 
void SampleArchiver::disconnect(SampleInput* inputarg) throw(atdUtil::IOException)
{
    if (!input) return;

    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s is disconnecting from %s",
	inputarg->getName().c_str(),getName().c_str());

    assert(input == inputarg);

    set<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
        inputarg->removeSampleClient(output);
        output->flush();
        output->close();
    }
    input = 0;
}
 
void SampleArchiver::connected(SampleOutput* output) throw()
{
    addOutput(output);
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	output->getName().c_str(),
	getName().c_str());

    try {
	output->init();
    }
    catch( const atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: error: %s",
	    output->getName().c_str(),ioe.what());
	disconnected(output);
	return;
    }
    assert(input);
    cerr << input->getName() << " addSampleClient " << output->getName() << endl;
    input->addSampleClient(output);
}
 
void SampleArchiver::disconnected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	output->getName().c_str(),
	getName().c_str());
    assert(input);
    input->removeSampleClient(output);
    output->close();
}

void SampleArchiver::newFileCallback(dsm_time_t thead,IOStream* iostream)
	throw(atdUtil::IOException)
{

    cerr << "SampleArchiver::newFileCallback" << endl;
    SampleFileHeader header;
    header.setArchiveVersion(Version::getArchiveVersion());
    header.setSoftwareVersion(Version::getSoftwareVersion());
    header.setProjectName(Project::getInstance()->getName());
    header.setXMLName(Project::getInstance()->getXMLName());
    header.setXMLVersion(Project::getInstance()->getVersion());
    header.write(iostream);
}

