/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_RAWSAMPLESERVICE_H
#define DSM_RAWSAMPLESERVICE_H

#include <DSMService.h>
#include <SampleInput.h>
#include <SampleIOProcessor.h>

namespace dsm {

/**
 * A RawSampleService reads raw Samples from a socket connection
 * and sends the samples to one or more SampleClients.
 */
class RawSampleService: public DSMService, public SampleConnectionRequester
{
public:
    RawSampleService();

    /**
     * Copy constructor.
     */
    RawSampleService(const RawSampleService&);

    ~RawSampleService();

    DSMService* clone() const;

    int run() throw(atdUtil::Exception);

    void connected(SampleInput*) throw();
    void disconnected(SampleInput*) throw();

    void schedule() throw(atdUtil::Exception);

    /**
     * Override setDSMConfig to set the DSMConfig value of
     * my inputs and processors.
     */
    void setDSMConfig(const DSMConfig* val);

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

    SampleInput* input;

    std::list<SampleIOProcessor*> processors;

};

}

#endif
