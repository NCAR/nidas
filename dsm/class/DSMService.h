/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_DSMSERVICE_H
#define DSM_DSMSERVICE_H

#include <atdUtil/McSocket.h>
#include <atdUtil/Thread.h>
#include <DOMable.h>
#include <SampleInput.h>
#include <SampleOutput.h>
#include <SampleIOProcessor.h>
// #include <ConnectionRequester.h>
#include <Site.h>

namespace dsm {

class DSMServer;
class Site;

/**
 * Base class for a service, as built from a <service> XML tag.
 */
class DSMService: public atdUtil::Thread, public SampleConnectionRequester,
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
    DSMService(const DSMService& x,SampleInputStream*);

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
    virtual void schedule() throw(atdUtil::Exception) = 0;

    virtual int checkSubServices() throw();
    virtual void cancel() throw();
    virtual void interrupt() throw();
    virtual int join() throw();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    void addSubService(DSMService*) throw();

    DSMServer* server;

    std::set<DSMService*> subServices;

    atdUtil::Mutex subServiceMutex;

    SampleInputStream* input;

    std::list<SampleIOProcessor*> processors;

};

}

#endif
