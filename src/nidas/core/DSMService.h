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

#include <nidas/util/McSocket.h>
#include <nidas/util/Thread.h>
#include <nidas/core/DOMable.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/core/SampleOutput.h>
#include <nidas/core/SampleIOProcessor.h>
// #include <nidas/core/ConnectionRequester.h>
#include <nidas/core/Site.h>

namespace nidas { namespace core {

class DSMServer;
class Site;

/**
 * Base class for a service, as built from a <service> XML tag.
 */
class DSMService: public nidas::util::Thread, public SampleConnectionRequester,
	public DOMable
{
public:
    
    /**
     * Constructor.
     */
    DSMService(const std::string& name);

    /**
     * Copy constuctor.
     */
    DSMService(const DSMService& x);

    /**
     * Copy constuctor with a new input.
     */
    DSMService(const DSMService& x,nidas::dynld::SampleInputStream*);

    virtual ~DSMService();

    virtual void setDSMServer(DSMServer* val);

    virtual DSMServer* getDSMServer() const { return server; }

    /**
     * Add a processor to this RawSampleService. This is done
     * at configuration (XML) time.
     */
    virtual void addProcessor(SampleIOProcessor* proc)
    {
        processors.push_back(proc);
    }

    virtual const std::list<SampleIOProcessor*>& getProcessors() const
    {
        return processors;
    }

    ProcessorIterator getProcessorIterator() const;

    /**
     * schedule this service to run.
     */
    virtual void schedule() throw(nidas::util::Exception) = 0;

    virtual int checkSubServices() throw();
    virtual void cancel() throw();
    virtual void interrupt() throw();
    virtual int join() throw();

    static const std::string getClassName(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    void addSubService(DSMService*) throw();

    DSMServer* server;

    std::set<DSMService*> subServices;

    nidas::util::Mutex subServiceMutex;

    nidas::dynld::SampleInputStream* input;

    std::list<SampleIOProcessor*> processors;

};

}}	// namespace nidas namespace core

#endif
