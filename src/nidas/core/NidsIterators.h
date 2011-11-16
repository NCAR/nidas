// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
class Variable;

/**
 * Class for iterating over the Sites of a Project,
 * or the Sites served by a DSMServer.
 */
class SiteIterator
{
public:
    SiteIterator(const Project*);

    SiteIterator();

    bool hasNext();

    Site* next() { return *_siteItr++; }

private:

    const std::list<Site*>* _sites;

    std::list<Site*>::const_iterator _siteItr;
};

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

    DSMServer* next() { return *_serverItr++; }

private:

    const std::list<DSMServer*>* _servers;

    std::list<DSMServer*>::const_iterator _serverItr;

    SiteIterator _siteIterator;
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

    DSMService* next() { return *_dsmServiceItr++; }

private:

    DSMServerIterator _dsmServerIterator;

    const std::list<DSMService*>* _services;

    std::list<DSMService*>::const_iterator _dsmServiceItr;
};

class SampleIOProcessor;

class DSMConfig;


/**
 * Class for iterating over the DSMConfigs of a Project or
 * Site.
 */
class DSMConfigIterator
{
public:
    DSMConfigIterator(const Project*);

    DSMConfigIterator(const Site*);

    DSMConfigIterator();

    bool hasNext();

    const DSMConfig* next() { return *_dsmItr++; }

private:

    SiteIterator _siteIterator;

    const std::list<const DSMConfig*>* _dsms;

    std::list<const DSMConfig*>::const_iterator _dsmItr;
};

/**
 * Class for iterating over the Processors of a DSMServer or DSMConfig.
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

    SampleIOProcessor* next() { return *_procItr++; }

private:

    DSMServiceIterator _serviceIterator;

    DSMConfigIterator _dsmIterator;

    const std::list<SampleIOProcessor*>* _processors;

    std::list<SampleIOProcessor*>::const_iterator _procItr;
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

    SensorIterator();

    bool hasNext();

    DSMSensor* next() { return *_sensorItr++; }

private:

    DSMConfigIterator _dsmIterator;

    const std::list<DSMSensor*>* _sensors;

    std::list<DSMSensor*>::const_iterator _sensorItr;
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

    /**
     * Copy constructor.
     */
    SampleTagIterator(const SampleTagIterator&x):
        _sensorIterator(x._sensorIterator),
        _processorIterator(x._processorIterator),
        _stags(x._stags),_sampleTagItr(_stags.begin())
    {
        /* Must define a copy constructor since _stags
         * is a list and not a pointer. The default copy
         * constructor is OK for all the other iterators, since they
         * contain a pointer to a container which is part of
         * Project::getInstance(), and so remains valid
         * (hopefully) over the life of the iterator.
         * In this class _stags is a new list, so we must reset
         * _sampleTagItr to point to the beginning of the new list.
         */
    }

    SampleTagIterator& operator=(const SampleTagIterator&x)
    {
        /* Must define an assignment operator for the same reason
         * mentioned above in the copy constructor.
         */
        if (this != &x) {
            _sensorIterator = x._sensorIterator;
            _stags = x._stags;
            _sampleTagItr = _stags.begin();
        }
        return *this;
    }
        
    SampleTagIterator(const Project*);

    SampleTagIterator(const DSMServer*);

    SampleTagIterator(const DSMService*);

    SampleTagIterator(const Site*);

    SampleTagIterator(const DSMConfig*);

    SampleTagIterator(const SampleSource*);

    SampleTagIterator();

    bool hasNext();

    const SampleTag* next() { return *_sampleTagItr++; }

private:

    SensorIterator _sensorIterator;

    ProcessorIterator _processorIterator;

    std::list<const SampleTag*> _stags;

    std::list<const SampleTag*>::const_iterator _sampleTagItr;
};

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

    /**
     * Excplicit copy constructor to satisfy -Weffc++.
     */
    VariableIterator(const VariableIterator& x):
        _sampleTagIterator(x._sampleTagIterator),
        _variables(x._variables),
        _variableItr(x._variableItr)
    {
    }

    /**
     * Excplicit assignment operator to satisfy -Weffc++.
     */
    VariableIterator& operator=(const VariableIterator& rhs)
    {
        if (&rhs != this) {
            _sampleTagIterator = rhs._sampleTagIterator;
            _variables = rhs._variables;
            _variableItr = rhs._variableItr;
        }
        return *this;
    }

    bool hasNext();

    const Variable* next() { return *_variableItr++; }

private:

    SampleTagIterator _sampleTagIterator;

    const std::vector<const Variable*>* _variables;

    std::vector<const Variable*>::const_iterator _variableItr;
};

}}	// namespace nidas namespace core

#endif
