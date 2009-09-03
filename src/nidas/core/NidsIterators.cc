
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
#include <nidas/core/NidsIterators.h>

using namespace nidas::core;
using namespace std;

DSMServerIterator::DSMServerIterator(const Project* obj):
	servers(&obj->getServers()),
	itr1(servers->begin()),
	sites(&obj->getSites()),
	itr2(sites->begin())
{
}

DSMServerIterator::DSMServerIterator(const Site* obj):
	servers(&obj->getServers()),
	itr1(servers->begin()),
	sites(0) {}

DSMServerIterator::DSMServerIterator():
	servers(0),sites(0) {}

bool DSMServerIterator::hasNext()
{
    if (!servers) return false;
    for ( ; itr1 == servers->end(); ) {
        if (! sites || itr2 == sites->end()) return false;
	Site* site = *itr2++;
	servers = &site->getServers();
	itr1 = servers->begin();
    }
    return true;
}

DSMServiceIterator::DSMServiceIterator(const Project* obj):
	itr1(obj->getDSMServerIterator()),services(0) {}

DSMServiceIterator::DSMServiceIterator(const Site* obj):
	itr1(obj->getDSMServerIterator()),services(0) {}

DSMServiceIterator::DSMServiceIterator(const DSMServer* obj):
	services(&obj->getServices()),itr2(services->begin()) {}

DSMServiceIterator::DSMServiceIterator(): services(0) {}

bool DSMServiceIterator::hasNext()
{
    if (services && itr2 != services->end()) return true;
    for (; itr1.hasNext(); ) {
	const DSMServer* server = itr1.next();
	services = &server->getServices();
	itr2 = services->begin();
	if (itr2 != services->end()) return true;
    }
    return false;
}

ProcessorIterator::ProcessorIterator(const Project* obj):
	itr1(obj->getDSMServiceIterator()),processors(0) {}

ProcessorIterator::ProcessorIterator(const Site* obj):
	itr1(obj->getDSMServiceIterator()),processors(0) {}

ProcessorIterator::ProcessorIterator(const DSMServer* obj):
	itr1(obj->getDSMServiceIterator()),processors(0) {}

ProcessorIterator::ProcessorIterator(const DSMService* obj):
	processors(&obj->getProcessors()),itr2(processors->begin()) {}

ProcessorIterator::ProcessorIterator(const DSMConfig* obj):
	processors(&obj->getProcessors()),itr2(processors->begin()) {}

ProcessorIterator::ProcessorIterator(): processors(0) {}

bool ProcessorIterator::hasNext()
{
    if (processors && itr2 != processors->end()) return true;
    for (; itr1.hasNext(); ) {
	const DSMService* service = itr1.next();
	processors = &service->getProcessors();
	itr2 = processors->begin();
	if (itr2 != processors->end()) return true;
    }
    return false;
}


SiteIterator::SiteIterator(const Project* obj):
	sites(&obj->getSites()),itr2(sites->begin()) {}

SiteIterator::SiteIterator(const DSMServer* obj):
	sites(&obj->getSites()),itr2(sites->begin()) {}

SiteIterator::SiteIterator(): sites(0) {}

bool SiteIterator::hasNext()
{
    if (!sites) return false;
    return itr2 != sites->end();
}

DSMConfigIterator::DSMConfigIterator(const Project* obj):
	itr1(obj->getSiteIterator()),dsms(0) {}

DSMConfigIterator::DSMConfigIterator(const DSMServer* obj):
	itr1(obj->getSiteIterator()),dsms(0) {}

DSMConfigIterator::DSMConfigIterator(const Site* site):
	itr1(),dsms(&site->getDSMConfigs()),itr2(dsms->begin()) {}

DSMConfigIterator::DSMConfigIterator():itr1(), dsms(0) {}

bool DSMConfigIterator::hasNext()
{
    if (dsms && itr2 != dsms->end()) return true;
    for (; itr1.hasNext(); ) {
	const Site* site = itr1.next();
	dsms = &site->getDSMConfigs();
	itr2 = dsms->begin();
	if (itr2 != dsms->end()) return true;
    }
    return false;
}

SensorIterator::SensorIterator(const Project* obj):
	itr1(obj->getDSMConfigIterator()),sensors(0) {}

SensorIterator::SensorIterator(const Site* obj):
	itr1(obj->getDSMConfigIterator()),sensors(0) {}

SensorIterator::SensorIterator(const DSMServer* obj):
	itr1(obj->getDSMConfigIterator()),sensors(0) {}

SensorIterator::SensorIterator(const DSMConfig* obj):
	itr1(),sensors(&obj->getSensors()),itr2(sensors->begin()) {}

SensorIterator::SensorIterator():itr1(), sensors(0) {}

bool SensorIterator::hasNext()
{
    if (sensors && itr2 != sensors->end()) return true;
    for (; itr1.hasNext(); ) {
	const DSMConfig* dsm = itr1.next();
	sensors = &dsm->getSensors();
	itr2 = sensors->begin();
	if (itr2 != sensors->end()) return true;
    }
    return false;
}


SampleTagIterator::SampleTagIterator(const Project* obj):
	itr1(obj->getSensorIterator()),itr2(stags.end()) {}

SampleTagIterator::SampleTagIterator(const Site* obj):
	itr1(obj->getSensorIterator()),itr2(stags.end()) {}

SampleTagIterator::SampleTagIterator(const DSMConfig* obj):
	itr1(obj->getSensorIterator()),itr2(stags.end()) {}

SampleTagIterator::SampleTagIterator(const DSMServer* obj):
	itr1(obj->getSensorIterator()),itr2(stags.end()) {}

SampleTagIterator::SampleTagIterator(const SampleSource* obj):
	itr1(),stags(obj->getSampleTags()),itr2(stags.begin()) {}

SampleTagIterator::SampleTagIterator():itr1(),itr2(stags.end()) {}

bool SampleTagIterator::hasNext()
{
    if (itr2 != stags.end()) return true;
    for (; itr1.hasNext(); ) {
	DSMSensor* sensor = itr1.next();
	stags = sensor->getSampleTags();
	itr2 = stags.begin();
	if (itr2 != stags.end()) return true;
    }
    return false;
}


VariableIterator::VariableIterator(const Project* obj):
	itr1(obj->getSampleTagIterator()),variables(0) {}

VariableIterator::VariableIterator(const Site* obj):
	itr1(obj->getSampleTagIterator()),variables(0) {}

VariableIterator::VariableIterator(const DSMConfig* obj):
	itr1(obj->getSampleTagIterator()),variables(0) {}

VariableIterator::VariableIterator(const SampleSource* obj):
	itr1(obj->getSampleTagIterator()),variables(0) {}

VariableIterator::VariableIterator(const SampleTag* stag):
        itr1(),variables(&stag->getVariables()),itr2(variables->begin()) {}

bool VariableIterator::hasNext()
{
    if (variables && itr2 != variables->end()) return true;
    for (; itr1.hasNext(); ) {
	const SampleTag* stag = itr1.next();
	variables = &stag->getVariables();
	itr2 = variables->begin();
	if (itr2 != variables->end()) return true;
    }
    return false;
}

