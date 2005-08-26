/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_RAWSAMPLESERVICE_H
#define DSM_RAWSAMPLESERVICE_H

#include <DSMService.h>
#include <RawSampleInputStream.h>
#include <SampleIOProcessor.h>

namespace dsm {

/**
 * A RawSampleService reads raw Samples from a socket connection
 * and sends the samples to one or more SampleClients.
 */
class RawSampleService: public DSMService
{
public:
    RawSampleService();

    /**
     * Copy constructor, but with a new input.
     */
    RawSampleService(const RawSampleService&,SampleInputStream* newinput);

    ~RawSampleService();

    // DSMService* clone() const;

    int run() throw(atdUtil::Exception);

    void interrupt() throw();

    void connected(SampleInput*) throw();

    void disconnected(SampleInput*) throw();

    void schedule() throw(atdUtil::Exception);

    /**
     * Add a processor to this RawSampleService. This is done
     * at configuration (XML) time.
     */
    void addProcessor(SampleIOProcessor* proc) { processors.push_back(proc); }

    const std::list<SampleIOProcessor*>& getProcessors() const { return processors; }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);


protected:

    SampleInputStream* input;

    SampleInputMerger merger;

    std::list<SampleIOProcessor*> processors;

};

}

#endif
