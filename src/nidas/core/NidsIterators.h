/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef NIDAS_CORE_NIDSITERATORS_H
#define NIDAS_CORE_NIDSITERATORS_H

#include <list>
// #include <set>
#include <vector>

namespace nidas { namespace core {

class Project;
class DSMServer;
class DSMService;
class Site;

/**
 * Class for iterating over the DSMServers of a Project.
 */
class DSMServerIterator
{
public:
    DSMServerIterator(const Project*);

    DSMServerIterator(const Site*);

    DSMServerIterator();

    bool hasNext();

    DSMServer* next() { return *itr1++; }

private:

    const std::list<DSMServer*>* servers;

    std::list<DSMServer*>::const_iterator itr1;

    const std::list<Site*>* sites;

    std::list<Site*>::const_iterator itr2;
};

/**
 * Class for iterating over the DSMServices of a DSMServer.
 */
class DSMServiceIterator
{
public:
    DSMServiceIterator(const Project*);

    DSMServiceIterator(const Site*);

    DSMServiceIterator(const DSMServer*);

    DSMServiceIterator();

    bool hasNext();

    DSMService* next() { return *itr2++; }

private:

    DSMServerIterator itr1;

    const std::list<DSMService*>* services;

    std::list<DSMService*>::const_iterator itr2;
};

class SampleIOProcessor;

class DSMConfig;

/**
 * Class for iterating over the Processors of a DSMServer.
 */
class ProcessorIterator
{
public:
    ProcessorIterator(const Project*);

    ProcessorIterator(const Site*);

    ProcessorIterator(const DSMServer*);

    ProcessorIterator(const DSMService*);

    ProcessorIterator(const DSMConfig*);

    ProcessorIterator();

    bool hasNext();

    SampleIOProcessor* next() { return *itr2++; }

private:

    DSMServiceIterator itr1;

    const std::list<SampleIOProcessor*>* processors;

    std::list<SampleIOProcessor*>::const_iterator itr2;
};

/**
 * Class for iterating over the Sites of a Project,
 * or the Sites served by a DSMServer.
 */
class SiteIterator
{
public:
    SiteIterator(const Project*);

    SiteIterator(const DSMServer*);

    SiteIterator();

    bool hasNext();

    Site* next() { return *itr2++; }

private:

    const std::list<Site*>* sites;

    std::list<Site*>::const_iterator itr2;
};


/**
 * Class for iterating over the DSMConfigs of a Project or
 * Site.
 */
class DSMConfigIterator
{
public:
    DSMConfigIterator(const Project*);

    DSMConfigIterator(const Site*);

    DSMConfigIterator(const DSMServer*);

    DSMConfigIterator();

    bool hasNext();

    const DSMConfig* next() { return *itr2++; }

private:

    SiteIterator itr1;

    const std::list<const DSMConfig*>* dsms;

    std::list<const DSMConfig*>::const_iterator itr2;
};

class DSMSensor;

/**
 * Class for iterating over the DSMSensors of a Project,
 * Site, or DSMConfig.
 */
class SensorIterator
{
public:
    SensorIterator(const Project*);

    SensorIterator(const Site*);

    SensorIterator(const DSMConfig*);

    SensorIterator(const DSMServer*);

    SensorIterator();

    bool hasNext();

    DSMSensor* next() { return *itr2++; }

private:

    DSMConfigIterator itr1;

    const std::list<DSMSensor*>* sensors;

    std::list<DSMSensor*>::const_iterator itr2;
};

class SampleSource;
class SampleIOProcessor;
class SampleTag;

/**
 * Class for iterating over the SampleTags of a Project,
 * Site, DSMConfig, or a SampleSource.
 */
class SampleTagIterator
{
public:
    SampleTagIterator(const Project*);

    SampleTagIterator(const DSMServer*);

    SampleTagIterator(const DSMService*);

    SampleTagIterator(const Site*);

    SampleTagIterator(const DSMConfig*);

    SampleTagIterator(const SampleSource*);

    SampleTagIterator();

    bool hasNext();

    const SampleTag* next() { return *itr2++; }

private:

    SensorIterator itr1;

    std::list<const SampleTag*> stags;

    std::list<const SampleTag*>::const_iterator itr2;
};

class Variable;

/**
 * Class for iterating over the Variables of a Project,
 * Site, DSMConfig, DSMSensor, or SampleTag.
 */
class VariableIterator
{
public:
    VariableIterator(const Project*);

    VariableIterator(const DSMServer*);

    VariableIterator(const DSMService*);

    VariableIterator(const Site*);

    VariableIterator(const DSMConfig*);

    VariableIterator(const SampleSource*);

    VariableIterator(const SampleTag*);

    // VariableIterator();

    bool hasNext();

    const Variable* next() { return *itr2++; }

private:

    SampleTagIterator itr1;

    const std::vector<const Variable*>* variables;

    std::vector<const Variable*>::const_iterator itr2;
};

}}	// namespace nidas namespace core

#endif
