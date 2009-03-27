/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_DYNLD_SAMPLEOUTPUTSTREAM_H
#define NIDAS_DYNLD_SAMPLEOUTPUTSTREAM_H


#include <nidas/core/SampleOutput.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * A class for serializing Samples on an OutputStream.
 */
class SampleOutputStream: public SampleOutputBase
{
public:

    SampleOutputStream(IOChannel* iochan=0);

    /**
     * Copy constructor.
     */
    SampleOutputStream(const SampleOutputStream&);

    /**
     * Copy constructor, with a new IOChannel.
     */
    SampleOutputStream(const SampleOutputStream&,IOChannel*);

    virtual ~SampleOutputStream();

    SampleOutputStream* clone(IOChannel* iochannel=0) const;

    /**
     * Get the IOStream of this SampleOutputStream.
     * SampleOutputStream owns the pointer and
     * will delete the IOStream in its destructor.
     * The IOStream is available after the
     * call to init() and before close() (or the destructor).
     */
    IOStream* getIOStream() { return _iostream; }

    /**
     * Call init() after the IOChannel is configured for a
     * SampleOutputStream. init() creates the buffered IOStream.
     */
    void init() throw();

    void close() throw(nidas::util::IOException);

    bool receive(const Sample *s) throw();

    void finish() throw();

    size_t write(const void* buf, size_t len)
    	throw(nidas::util::IOException);

    long long getNumOutputBytes() const
    {
        if (_iostream) return _iostream->getNumOutputBytes();
        return 0;
    }

    size_t getNumOutputSamples() const { return _nsamples; }

    dsm_time_t getLastOutputTimeTag() const { return _lastTimeTag; }

    void setLastOutputTimeTag(dsm_time_t val) { _lastTimeTag = val; }

protected:

    void incrementNumOutputSamples() { _nsamples++; }

    size_t write(const Sample* samp) throw(nidas::util::IOException);

    IOStream* _iostream;

private:

    size_t _nsamplesDiscarded;

    long long _nsamples;

    dsm_time_t _lastTimeTag;

};

/**
 * A proxy for a SampleOutputStream. One passes a reference to a
 * SampleOutputStream to the constructor for this proxy.
 * The SampleOutputStreamProxy::receive method
 * will invoke the SampleOutputStream::receive() method not the
 * derived method.
 */
class SampleOutputStreamProxy: public SampleClient
{
public:
    SampleOutputStreamProxy(SampleOutputStream& out):
    	outstream(out) {}
    bool receive(const Sample* samp) throw()
    {
	// cerr << "Proxy receive" << endl;
        return outstream.SampleOutputStream::receive(samp);
    }
private:
    SampleOutputStream& outstream;
};

/**
 * A class for serializing Samples on an OutputStream.
 */
class SortedSampleOutputStream: public SampleOutputStream
{
public:

    SortedSampleOutputStream();

    /**
     * Copy constructor.
     */
    SortedSampleOutputStream(const SortedSampleOutputStream&);

    /**
     * Copy constructor, with a new IOChannel.
     */
    SortedSampleOutputStream(const SortedSampleOutputStream&,IOChannel*);

    virtual ~SortedSampleOutputStream();

    SortedSampleOutputStream* clone(IOChannel* iochannel=0) const;

    void init() throw();

    bool receive(const Sample *s) throw();

    void finish() throw();

    /**
     * Set length of SampleSorter, in milliseconds.
     */
    void setSorterLengthMsecs(int val)
    {
        sorterLengthMsecs = val;
    }

    int getSorterLengthMsecs() const
    {
        return sorterLengthMsecs;
    }

    /**
     * Set the maximum amount of heap memory to use for sorting samples.
     * @param val Maximum size of heap in bytes.
     * @see SampleSorter::setHeapMax().
     */
    void setHeapMax(size_t val)
    {
        heapMax = val;
    }

    size_t getHeapMax() const
    {
        return heapMax;
    }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);


private:

    SampleSorter* sorter;

    SampleOutputStreamProxy proxy;

    /**
     * Length of SampleSorter, in milli-seconds.
     */
    int sorterLengthMsecs;

    size_t heapMax;

};

}}	// namespace nidas namespace core

#endif
