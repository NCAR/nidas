
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$


*/

#ifndef NIDAS_CORE_SAMPLESOURCE_H
#define NIDAS_CORE_SAMPLESOURCE_H

#include <nidas/core/SampleTag.h>
#include <nidas/core/SampleClientList.h>

#include <set>

namespace nidas { namespace core {

/**
 * A source of samples. A SampleSource maintains a list
 * of SampleClients.  When a SampleSource has a Sample ready,
 * it will call the receive method of all its SampleClients.
 * SampleClients register/unregister with a SampleSource via
 * the addSampleClient/removeSampleClient methods.
 */
class SampleSource {
public:

    SampleSource(): _numDistributedSamples(0),_numDistributedBytes(0),
        _lastDistributedTimeTag(0L)
    {
    }

    virtual ~SampleSource() {}

    /**
     * Add a SampleClient to this SampleSource.  The pointer
     * to the SampleClient must remain valid, until after
     * it is removed.
     */
    virtual void addSampleClient(SampleClient* c) throw() {
        _clients.add(c);
    }

    /**
     * Remove a SampleClient from this SampleSource
     */
    virtual void removeSampleClient(SampleClient* c) throw() {
        _clients.remove(c);
    }

    /**
     * How many SampleClients are currently in my list.
     */
    virtual int getClientCount() const throw() {
        return _clients.size();
    }

    /**
     * Big cleanup.
     */
    virtual void removeAllSampleClients() throw() {
        _clients.removeAll();
    }

    /**
     * Distribute a sample to my clients, calling the receive() method
     * of each client, passing the const pointer to the Sample.
     * Does a s->freeReference() on the sample when done,
     * which means you should assume that the pointer to the Sample
     * is invalid after the call to distribute(),
     * unless the owner has done an additional holdReference().
     * If so, it is very bad form to make any changes
     * to Sample after the distribute() - the clients may get
     * a half-changed Sample.
     */
    virtual void distribute(const Sample* s) throw();

    /**
     * Distribute a list of samples to my clients. Calls receive() method
     * of each client, passing the pointer to the Sample.
     * Does a s->freeReference() on each sample in the list.
     */
    virtual void distribute(const std::list<const Sample*>& samps) throw();

    /**
     * Request that this SampleSource flush it's buffers.
     * Default implementation passes a finish() request
     * onto all the clients.
     */
    virtual void flush() throw();

    virtual const std::list<const SampleTag*>& getSampleTags() const = 0;

    virtual SampleTagIterator getSampleTagIterator() const;

    /**
     * How many samples have been distributed by this SampleSource.
     */
    size_t getNumDistributedSamples() const {
        return _numDistributedSamples;
    }

    /**
     * How many bytes have been distributed by this SampleSource.
     */
    long long getNumDistributedBytes() const {
        return _numDistributedBytes;
    }

    /**
     * Timetag of most recent sample distributed by the merger.
     */
    dsm_time_t getLastDistributedTimeTag() const
    {
        return _lastDistributedTimeTag;
    }

private:

    /**
     * My current clients.
     */
    SampleClientList _clients;

    /**
     * Number of samples distributed.
     */
    size_t _numDistributedSamples;

    long long _numDistributedBytes;

    dsm_time_t _lastDistributedTimeTag;

};

}}	// namespace nidas namespace core


#endif
