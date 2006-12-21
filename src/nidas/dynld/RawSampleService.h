/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef NIDAS_DYNLD_RAWSAMPLESERVICE_H
#define NIDAS_DYNLD_RAWSAMPLESERVICE_H

#include <nidas/core/DSMService.h>
#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/core/SampleIOProcessor.h>

namespace nidas { namespace dynld {

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

    int run() throw(nidas::util::Exception);

    void interrupt() throw();

    void connected(SampleInput*) throw();

    void disconnected(SampleInput*) throw();

    void schedule() throw(nidas::util::Exception);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    SampleInputMerger* merger;

};

}}	// namespace nidas namespace core

#endif
