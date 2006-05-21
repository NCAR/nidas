
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$


*/

#ifndef DSM_SAMPLESOURCE_H
#define DSM_SAMPLESOURCE_H

#include <SampleTag.h>
#include <SampleClientList.h>

namespace dsm {

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
     * Add a SampleClient to this SampleSource.  The pointer
     * to the SampleClient must remain valid, until after
     * it is removed.
     */
    virtual void addSampleClient(SampleClient* c) throw() {
        clients.add(c);
    }

    /**
     * Remove a SampleClient from this SampleSource
     */
    virtual void removeSampleClient(SampleClient* c) throw() {
        clients.remove(c);
    }

    /**
     * How many SampleClients are currently in my list.
     */
    virtual int getClientCount() const throw() {
        return clients.size();
    }

    /**
     * Big cleanup.
     */
    virtual void removeAllSampleClients() throw() {
        clients.removeAll();
    }

    /**
     * How many samples have been distributed by this SampleSource.
     */
    unsigned long getNumSamplesSent() const throw() {
        return numSamplesSent;
    }

    virtual void setNumSamplesSent(unsigned long val) throw() {
        numSamplesSent = val;
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

    virtual const std::set<const SampleTag*>& getSampleTags() const = 0;

    virtual SampleTagIterator getSampleTagIterator() const;

private:

    /**
     * My current clients.
     */
    SampleClientList clients;

    /**
     * Number of samples distributed.
     */
    int numSamplesSent;

};
}

#endif
