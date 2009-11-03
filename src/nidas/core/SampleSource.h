
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

#include <nidas/core/NidsIterators.h>
#include <nidas/core/SampleStats.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace core {

class SampleClient;
class SampleTag;
class Sample;

/**
 * A source of samples. A SampleSource maintains a list
 * of SampleClients.  When a SampleSource has a Sample ready,
 * it will call the receive method of all its SampleClients.
 * SampleClients register/unregister with a SampleSource via
 * the addSampleClient/removeSampleClient methods.
 */
class SampleSource {
public:

    virtual ~SampleSource() {}

    /**
     * Several objects in NIDAS can be both a SampleSource of raw Samples
     * and processed Samples. SampleClients use this method to
     * get a pointer to whatever sample source they are interested
     * in. Derived classes can return NULL if they are not
     * a SampleSource of raw samples.
     */
    virtual SampleSource* getRawSampleSource() = 0;

    /**
     * Several objects in NIDAS can be both a SampleSource of raw Samples
     * and processed Samples. SampleClients use this method to
     * get a pointer to whatever sample source they are interested
     * in. Derived classes can return NULL if they are not
     * a SampleSource of processed samples.
     */
    virtual SampleSource* getProcessedSampleSource() = 0;

    /**
     * Add a SampleTag to this SampleSource. This SampleSource
     * does not own the SampleTag.
     */
    virtual void addSampleTag(const SampleTag*)
        throw (nidas::util::InvalidParameterException) = 0;

    virtual void removeSampleTag(const SampleTag*) throw () = 0;

    /**
     * What SampleTags am I a SampleSource for?
     */
    virtual std::list<const SampleTag*> getSampleTags() const = 0;

    virtual SampleTagIterator getSampleTagIterator() const = 0;

    /**
     * Add a SampleClient of all Samples to this SampleSource.
     * The pointer to the SampleClient must remain valid, until after
     * it is removed.
     */
    virtual void addSampleClient(SampleClient* c) throw() = 0;

    /**
     * Remove a SampleClient from this SampleSource.
     */
    virtual void removeSampleClient(SampleClient* c) throw() = 0;

    /**
     * Add a SampleClient to this SampleSource.  The pointer
     * to the SampleClient must remain valid, until after
     * it is removed.
     */
    virtual void addSampleClientForTag(SampleClient* c,const SampleTag*) throw() = 0;
    /**
     * Remove a SampleClient for a given SampleTag from this SampleSource.
     * The pointer to the SampleClient must remain valid, until after
     * it is removed.
     */
    virtual void removeSampleClientForTag(SampleClient* c,const SampleTag*) throw() = 0;

    /**
     * How many SampleClients are currently in my list.
     */
    virtual int getClientCount() const throw() = 0;

#ifdef NEEDED
    /**
     * Big cleanup.
     */
    virtual void removeAllSampleClients() throw() = 0;

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
    virtual void distribute(const Sample* s) throw() = 0;

    /**
     * Distribute a list of samples to my clients. Calls receive() method
     * of each client, passing the pointer to the Sample.
     * Does a s->freeReference() on each sample in the list.
     */
    virtual void distribute(const std::list<const Sample*>& samps) throw() = 0;
#endif

    /**
     * Request that this SampleSource flush it's buffers.
     * Default implementation issues a finish() request
     * to all the clients.
     */
    virtual void flush() throw() = 0;

    virtual const SampleStats& getSampleStats() const = 0;

};

}}	// namespace nidas namespace core


#endif
