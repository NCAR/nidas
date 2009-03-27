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
#include <nidas/dynld/SampleArchiver.h>
#include <nidas/core/SampleIOProcessor.h>

namespace nidas { namespace dynld {

/**
 * A RawSampleService reads raw Samples from a socket connection
 * and sends the samples to one or more SampleIOProcessors.
 */
class RawSampleService: public DSMService
{
public:
    RawSampleService();

    ~RawSampleService();

    void connected(SampleInput*) throw();

    void disconnected(SampleInput*) throw();

    void schedule() throw(nidas::util::Exception);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    void printClock(std::ostream& ostr) throw();

    void printStatus(std::ostream& ostr,float deltat) throw();

private:

    SampleInputMerger* _merger;

    /**
     * Worker thread that is run when a SampleInputConnection is established.
     */
    class Worker: public nidas::util::Thread
    {
        public:
            Worker(RawSampleService* svc,SampleInputStream *input);
            ~Worker();
            int run() throw(nidas::util::Exception);
        private:
            RawSampleService* _svc;
            SampleInputStream* _input;
            std::list<SampleIOProcessor*> _processors;
    };

    /**
     * Keep track of the Worker for each SampleInput.
     */
    std::map<SampleInput*,Worker*> _workers;

    std::map<SampleInput*,const DSMConfig*> _dsms;

    /**
     * Saved between calls to printStatus in order to compute data rates.
     */
    std::map<void*,size_t> _nsampsLast;

    /**
     * Saved between calls to printStatus in order to compute sample rates.
     */
    std::map<void*,long long> _nbytesLast;

    /**
     * Copying not supported.
     */
    RawSampleService(const RawSampleService&);

    /**
     * Assignment not supported.
     */
    RawSampleService& operator =(const RawSampleService&);
};

}}	// namespace nidas namespace dynld

#endif
