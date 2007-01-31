/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_CORE_SAMPLEINPUT_H
#define NIDAS_CORE_SAMPLEINPUT_H


#include <nidas/core/SampleSource.h>
#include <nidas/core/IOStream.h>
#include <nidas/core/ConnectionRequester.h>
#include <nidas/core/SampleSorter.h>
#include <nidas/core/SampleInputHeader.h>

namespace nidas { namespace core {

class DSMConfig;
class DSMSensor;

/**
 * Interface of an input SampleSource. Typically a SampleInput is
 * reading serialized samples from a socket or file, and
 * the sending them on.
 *
 */
class SampleInput: public SampleSource
{
public:

    virtual ~SampleInput() {}

    virtual std::string getName() const = 0;

    virtual nidas::util::Inet4Address getRemoteInet4Address() const = 0;

    /**
     * Client wants samples from the process() method of the
     * given DSMSensor.
     */
    virtual void addProcessedSampleClient(SampleClient*,DSMSensor*) = 0;

    virtual void removeProcessedSampleClient(SampleClient*,DSMSensor* = 0) = 0;

};

/**
 * SampleInputMerger sorts samples that are coming from one
 * or more inputs.  Samples can then be passed onto sensors
 * for processing, and then sorted again.
 *
 * SampleInputMerger makes use of two SampleSorters, inputSorter
 * and procSampleSorter.
 *
 * inputSorter is a client of one or more inputs (the text arrows
 * show the sample flow):
 *
 * input ----v
 * input --> inputSorter
 * input ----^
 *
 * After sorting the samples, inputSorter passes them onto the two
 * types of SampleClients that have registered with SampleInputMerger.
 * SampleClients that have registered with
 * SampleInputMerger::addSampleClient will receive their raw samples
 * directly from inputSorter.
 *
 * inputSorter -> sampleClients
 *
 * SampleClients that have registered with
 * SampleInputMerger::addProcessedSampleClient will receive their
 * samples indirectly:
 * 
 * inputSorter -> this -> sensor -> procSampSorter -> processedSampleClients
 *
 * inputSorter provides sorting of the samples from the various inputs.
 *
 * procSampSorter provides sorting of the processed samples.
 * Sensors are apt to create processed samples with different
 * time-tags than the input raw samples, therefore they need
 * to be sorted again.
 */
class SampleInputMerger: public SampleInput , protected SampleClient
{
public:
    SampleInputMerger();

    virtual ~SampleInputMerger();

    std::string getName() const { return name; }

    nidas::util::Inet4Address getRemoteInet4Address() const
    {
        return nidas::util::Inet4Address(INADDR_ANY);
    }

    /**
     * Add an input to be merged and sorted.
     */
    void addInput(SampleInput* input);

    void removeInput(SampleInput* input);

    /**
     * Add a SampleClient that wants samples which have been
     * merged from various inputs, sorted, processed through a
     * certain DSMSensor, and then re-sorted again.
     */
    void addProcessedSampleClient(SampleClient*,DSMSensor*);

    void removeProcessedSampleClient(SampleClient*,DSMSensor* = 0);

    /**
     * Add a SampleClient that wants samples which have been
     * merged from various inputs and then sorted.
     */
    void addSampleClient(SampleClient* client) throw();

    void removeSampleClient(SampleClient* client) throw();

    bool receive(const Sample*) throw();

    void addSampleTag(const SampleTag* stag);

    const std::set<const SampleTag*>& getSampleTags() const
    {
        return sampleTags;
    }

    /**
     * What DSMConfigs are associated with this SampleInput.
     */
    const std::list<const DSMConfig*>& getDSMConfigs() const
    {
        return dsmConfigs;
    }

protected:

    std::string name;

    std::map<unsigned long int, DSMSensor*> sensorMap;

    std::map<SampleClient*, std::list<DSMSensor*> > sensorsByClient;

    nidas::util::Mutex sensorMapMutex;

    SampleSorter inputSorter;

    SampleSorter procSampSorter;

    size_t unrecognizedSamples;

    std::set<const SampleTag*> sampleTags;

    std::list<const DSMConfig*> dsmConfigs;

};


/**
 * Extension of the interface to a SampleInput providing the
 * methods needed to establish the connection to the source
 * of samples (socket or files) and actually read samples
 * from the connection.
 */
class SampleInputReader: public SampleInput, public ConnectionRequester, public DOMable
{
public:

    virtual ~SampleInputReader() {}

    virtual void requestConnection(DSMService*)
        throw(nidas::util::IOException) = 0;

    virtual void init() throw(nidas::util::IOException) = 0;

    /**
     * Read a buffer of data, serialize the data into samples,
     * and distribute() samples to the receive() method of my SampleClients.
     * This will perform only one physical read of the underlying device
     * and so is appropriate to use when a select() has determined
     * that there is data availabe on our file descriptor.
     */
    virtual void readSamples() throw(nidas::util::IOException) = 0;

    /**
     * Blocking read of the next sample from the buffer. The caller must
     * call freeReference on the sample when they're done with it.
     */
    virtual Sample* readSample() throw(nidas::util::IOException) = 0;

    virtual void close() throw(nidas::util::IOException) = 0;

};

}}	// namespace nidas namespace core

#endif
