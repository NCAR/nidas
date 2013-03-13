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

#ifndef NIDAS_DYNLD_SAMPLEINPUTSTREAM_H
#define NIDAS_DYNLD_SAMPLEINPUTSTREAM_H

#include <nidas/core/SampleInput.h>
#include <nidas/core/SampleInputHeader.h>
#include <nidas/core/SampleSourceSupport.h>
#include <nidas/core/SampleStats.h>
#include <nidas/core/Sample.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/util/UTime.h>

namespace nidas {

namespace core {
class DSMService;
class DSMConfig;
class SampleTag;
class SampleStats;
class SampleClient;
class SampleSource;
class Sample;
class IOChannel;
class IOStream;
}

namespace dynld {

/**
 * An implementation of a SampleInput.
 *
 * The readSamples method converts raw bytes from the iochannel
 * into Samples.
 *
 * If a SampleClient has requested processed Samples via
 * addProcessedSampleClient, then SampleInputStream will pass
 * Samples to the respective DSMSensor for processing,
 * and then the DSMSensor passes the processed Samples to
 * the SampleClient:
 *
 * iochannel -> readSamples method -> DSMSensor -> SampleClient
 * 
 * If a SampleClient has requested non-processed Samples,
 * via the simple addSampleClient method, then SampleInput
 * stream passes the Samples straight to the SampleClient:
 *
 * iochannel -> readSamples method -> SampleClient
 *
 */
class SampleInputStream: public nidas::core::SampleInput
{
public:
    /**
     * Constructor.
     * @param raw Whether the input samples are raw.
     */
    SampleInputStream(bool raw=false);

    /**
     * Constructor.
     * @param iochannel The IOChannel that we use for data input.
     *   SampleInputStream will own the pointer to the IOChannel,
     *   and will delete it in ~SampleInputStream(). If 
     *   it is a null pointer, then it must be set within
     *   the fromDOMElement method.
     */
    SampleInputStream(nidas::core::IOChannel* iochannel,bool raw=false);

    /**
     * Create a clone, with a new, connected IOChannel.
     */
    virtual SampleInputStream* clone(nidas::core::IOChannel* iochannel);

    virtual ~SampleInputStream();

    std::string getName() const;

    /**
     * Set the IOChannel for this SampleInputStream.h
     */
    virtual void setIOChannel(nidas::core::IOChannel* val);

    /**
     * Read archive information at beginning of input stream or file.
     */
    void readInputHeader() throw(nidas::util::IOException);

    bool parseInputHeader() throw(nidas::util::IOException);

    const nidas::core::SampleInputHeader& getInputHeader() const
    {
        return _inputHeader;
    }

    void requestConnection(nidas::core::DSMService*) throw(nidas::util::IOException);

    virtual nidas::core::SampleInput* getOriginal() const
    {
        return _original;
    }

    /**
     * Implementation of IOChannelRequester::connected.
     * One can use this method to notify SampleInputStream that
     * the IOChannel is connected, which will cause SampleInputStream
     * to open the IOStream.
     */
    nidas::core::SampleInput* connected(nidas::core::IOChannel* iochan) throw();

    /**
     * What DSM am I connnected to? May be NULL if it cannot be determined.
     */
    const nidas::core::DSMConfig* getDSMConfig() const;

    // void init() throw();

    void setKeepStats(bool val)
    {
        _source.setKeepStats(val);
    }

    /**
     * Implementation of SampleInput::addSampleTag().
     */
    void addSampleTag(const nidas::core::SampleTag* tag) throw()
    {
        return _source.addSampleTag(tag);
    }

    void removeSampleTag(const nidas::core::SampleTag* tag) throw()
    {
        _source.removeSampleTag(tag);
    }

    nidas::core::SampleSource* getRawSampleSource()
    {
        return _source.getRawSampleSource();
    }

    nidas::core::SampleSource* getProcessedSampleSource()
    {
        return _source.getProcessedSampleSource();
    }

    /**
     * Get the output SampleTags.
     */
    std::list<const nidas::core::SampleTag*> getSampleTags() const
    {
        return _source.getSampleTags();
    }

    /**
     * Implementation of SampleSource::getSampleTagIterator().
     */
    nidas::core::SampleTagIterator getSampleTagIterator() const
    {
        return _source.getSampleTagIterator();
    }

    /**
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClient(nidas::core::SampleClient* client) throw()
    {
        _source.addSampleClient(client);
    }

    void removeSampleClient(nidas::core::SampleClient* client) throw()
    {
        _source.removeSampleClient(client);
    }

    /**
     * Add a Client for a given SampleTag.
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClientForTag(nidas::core::SampleClient* client,const nidas::core::SampleTag* tag) throw()
    {
        _source.addSampleClientForTag(client,tag);
    }

    void removeSampleClientForTag(nidas::core::SampleClient* client,const nidas::core::SampleTag* tag) throw()
    {
        _source.removeSampleClientForTag(client,tag);
    }

    int getClientCount() const throw()
    {
        return _source.getClientCount();
    }

    /**
     * Implementation of SampleSource::flush(), unpacks and distributes
     * any samples currently in the read buffer.
     */
    void flush() throw();

    const nidas::core::SampleStats& getSampleStats() const
    {
        return _source.getSampleStats();
    }

    /**
     * Read a buffer of data, serialize the data into samples,
     * and distribute() samples to the receive() method of my
     * SampleClients and DSMSensors.
     * This will perform only one physical read of the underlying
     * IOChannel and so is appropriate to use when a select()
     * has determined that there is data available on our file
     * descriptor, or when the physical device is configured
     * for non-blocking reads.
     */
    void readSamples() throw(nidas::util::IOException);

    /**
     * Search forward until a sample header is read whose time is 
     * greater than or equal to tt.  Leaves the InputStream
     * positioned so that the next call to readSample() or
     * readSamples() will read the rest of the sample.
     */
    void search(const nidas::util::UTime& tt) throw(nidas::util::IOException);

    /**
     * Read the next sample from the InputStream. The caller must
     * call freeReference on the sample when they're done with it.
     * This method may perform zero or more reads of the IOChannel.
     * @return pointer to a sample, never NULL.
     */
    nidas::core::Sample* readSample() throw(nidas::util::IOException);


    /**
     * Distribute a sample to my clients. One could use this
     * to insert a sample into the stream.
     */
    void distribute(const nidas::core::Sample* s) throw()
    {
        return _source.distribute(s);
    }

    size_t getBadSamples() const { return _badSamples; }

    void close() throw(nidas::util::IOException);

    void newFile() throw(nidas::util::IOException);

    void setFilterBadSamples(bool val)
    {
        _filterBadSamples = val;
    }

    void setMaxDsmId(int val)
    {
        _maxDsmId = val;
        setFilterBadSamples(val < 1024);
    }

    void setMaxSampleLength(unsigned int val)
    {
        _maxSampleLength = val;
        setFilterBadSamples(val < UINT_MAX);
    }

    void setMinSampleTime(const nidas::util::UTime& val)
    {
        _minSampleTime = val.toUsecs();
        setFilterBadSamples(val.toUsecs() > LONG_LONG_MIN);
    }

    void setMaxSampleTime(const nidas::util::UTime& val)
    {
        _maxSampleTime = val.toUsecs();
        setFilterBadSamples(val.toUsecs() < LONG_LONG_MAX);
    }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    void setExpectHeader(bool val) { _expectHeader = val; }

    bool getExpectHeader() const { return _expectHeader; }

protected:

    /**
     * Copy constructor, with a new, connected IOChannel.
     */
    SampleInputStream(SampleInputStream& x,nidas::core::IOChannel* iochannel);

    nidas::core::IOChannel* _iochan;

    nidas::core::SampleSourceSupport _source;

private:

    /**
     * Inpack the next sample from the InputStream.
     * This method does not perform any physical reads.
     * @return pointer to a sample if there is one available in the
     * buffer, else NULL.
     */
    nidas::core::Sample* nextSample() throw();

    /**
     * Service that has requested my input.
     */
    nidas::core::DSMService* _service;

    nidas::core::IOStream* _iostream;

    /* mutable */ const nidas::core::DSMConfig* _dsm;

    bool _expectHeader;

    bool _inputHeaderParsed;

    nidas::core::SampleHeader _sheader;

    size_t _headerToRead;

    char* _hptr;

    /**
     * Will be non-null if we have previously read part of a sample
     * from the stream.
     */
    nidas::core::Sample* _samp;

    /**
     * How many bytes left to read from the stream into the data
     * portion of samp.
     */
    size_t _dataToRead;

    /**
     * Pointer into the data portion of samp where we will read next.
     */
    char* _dptr;

    size_t _badSamples;

    nidas::core::SampleInputHeader _inputHeader;

    bool _filterBadSamples;

    unsigned int _maxDsmId;

    size_t _maxSampleLength;

    nidas::core::dsm_time_t _minSampleTime;

    nidas::core::dsm_time_t _maxSampleTime;

    SampleInputStream* _original;

    bool _raw;

    /**
     * No regular copy.
     */
    SampleInputStream(const SampleInputStream& x);

    /**
     * No assignment.
     */
    SampleInputStream& operator =(const SampleInputStream& x);

};

}}	// namespace nidas namespace dynld

#endif
