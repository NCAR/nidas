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
#include <RawSampleInputStream.h>

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

    void connected(SampleInput*);

    void connected(SampleOutput*);

    void schedule() throw(atdUtil::Exception);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:
    RawSampleInputStream* input;

    std::list<SampleOutput*> outputs;
};

}

#endif
