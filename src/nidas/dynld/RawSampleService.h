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
#include <nidas/dynld/SampleArchiver.h>
#include <nidas/core/SampleIOProcessor.h>
#include <nidas/core/SamplePipeline.h>

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

    void connect(SampleInput*) throw();

    void disconnect(SampleInput*) throw();

    /**
     * RawSampleService currently does not have directly connected
     * SampleOutputs. It has SampleIOProcessors, which receive
     * the SampleOutput connect and disconnect requests.
     * So this method does an assert(false).
     */
    void connect(SampleOutput*) throw() { assert(false); }

    /**
     * RawSampleService currently does not have directly connected
     * SampleOutputs. It has SampleIOProcessors, which receive
     * the SampleOutput connect and disconnect requests.
     * So this method does an assert(false).
     */
    void disconnect(SampleOutput*) throw() { assert(false); }

    void schedule() throw(nidas::util::Exception);

    void interrupt() throw();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    void printClock(std::ostream& ostr) throw();

    void printStatus(std::ostream& ostr,float deltat) throw();

    /**
     * Get the length of the SampleSorter of raw Samples, in seconds.
     */
    float getRawSorterLength() const
    {
        return _rawSorterLength;
    }

    /**
     * Set the length of the SampleSorter of raw Samples, in seconds.
     */
    void setRawSorterLength(float val)
    {
        _rawSorterLength = val;
    }

    /**
     * Get the length of the SampleSorter of processed Samples, in seconds.
     */
    float getProcSorterLength() const
    {
        return _procSorterLength;
    }

    /**
     * Set the length of the SampleSorter of processed Samples, in seconds.
     */
    void setProcSorterLength(float val)
    {
        _procSorterLength = val;
    }

    /**
     * Get the size of in bytes of the raw SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    size_t getRawHeapMax() const
    {
        return _rawHeapMax;
    }

    /**
     * Set the size of in bytes of the raw SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    void setRawHeapMax(size_t val)
    {
        _rawHeapMax = val;
    }

    /**
     * Get the size of in bytes of the processed SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    size_t getProcHeapMax() const
    {
        return _procHeapMax;
    }

    /**
     * Set the size of in bytes of the processed SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    void setProcHeapMax(size_t val)
    {
        _procHeapMax = val;
    }

private:

    SamplePipeline* _pipeline;

    /**
     * Worker thread that is run when a SampleInputConnection is established.
     */
    class Worker: public nidas::util::Thread
    {
        public:
            Worker(RawSampleService* svc,SampleInput *input);
            ~Worker();
            int run() throw(nidas::util::Exception);
        private:
            RawSampleService* _svc;
            SampleInput* _input;
    };

    /**
     * Keep track of the Worker for each SampleInput.
     */
    std::map<SampleInput*,Worker*> _workers;

    std::map<SampleInput*,const DSMConfig*> _dsms;

    nidas::util::Mutex _workerMutex;

    /**
     * Saved between calls to printStatus in order to compute data rates.
     */
    std::map<void*,size_t> _nsampsLast;

    /**
     * Saved between calls to printStatus in order to compute sample rates.
     */
    std::map<void*,long long> _nbytesLast;

    float _rawSorterLength;

    float _procSorterLength;

    size_t _rawHeapMax;

    size_t _procHeapMax;

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
