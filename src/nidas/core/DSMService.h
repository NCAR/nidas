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


#ifndef NIDAS_CORE_DSMSERVICE_H
#define NIDAS_CORE_DSMSERVICE_H

#include <nidas/util/Thread.h>
#include <nidas/core/DOMable.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/ConnectionRequester.h>

namespace nidas { namespace core {

class Project;
class DSMServer;
class Site;
class SampleInput;
class SampleIOProcessor;
class IOChannel;

/**
 * Base class for a service, as built from a <service> XML tag.
 */
class DSMService: public SampleConnectionRequester, public DOMable
{
public:
    
    /**
     * Constructor.
     */
    DSMService(const std::string& name);

    virtual ~DSMService();

    const std::string& getName() const
    {
        return _name;
    }

    virtual void setDSMServer(DSMServer* val);

    virtual DSMServer* getDSMServer() const { return _server; }

    virtual void connect(SampleInput*) throw() = 0;

    /**
     * Add a processor to this RawSampleService. This is done
     * at configuration (XML) time.
     */
    virtual void addProcessor(SampleIOProcessor* proc)
    {
        _processors.push_back(proc);
    }

    virtual const std::list<SampleIOProcessor*>& getProcessors() const
    {
        return _processors;
    }

    const std::list<SampleInput*>& getInputs() const
    {
        return _inputs;
    }

    ProcessorIterator getProcessorIterator() const;

    /**
     * schedule this service to run.
     */
    virtual void schedule(bool optionalProcessing) throw(nidas::util::Exception) = 0;

    virtual int checkSubThreads() throw();

    virtual void cancel() throw();

    virtual void interrupt() throw();

    virtual int join() throw();

    static const std::string getClassName(const xercesc::DOMElement* node,
        const Project*)
	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    nidas::util::Thread::SchedPolicy getSchedPolicy() const
    {
        return _threadPolicy;
    }

    int getSchedPriority() const
    {
        return _threadPriority;
    }

    virtual void printClock(std::ostream&) throw() {}

    virtual void printStatus(std::ostream&,float) throw() {}

protected:

    void addSubThread(nidas::util::Thread*) throw();

    std::string _name;

    DSMServer* _server;

    std::set<nidas::util::Thread*> _subThreads;

    nidas::util::Mutex _subThreadMutex;

    std::list<SampleInput*> _inputs;

    std::list<SampleIOProcessor*> _processors;

    std::list<IOChannel*> _ochans;

    nidas::util::Thread::SchedPolicy _threadPolicy;

    int _threadPriority;

private:
    /**
     * Copying not supported.
     */
    DSMService(const DSMService& x);

    /**
     * Assignment not supported.
     */
    DSMService& operator = (const DSMService& x);

};

}}	// namespace nidas namespace core

#endif
