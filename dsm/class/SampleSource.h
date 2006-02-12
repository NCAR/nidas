
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

#include <SampleSourceImpl.h>

namespace dsm {

/**
 * A source of samples. A SampleSource maintains a list
 * of SampleClients.  When a SampleSource has a Sample ready,
 * it will call the receive method of all its SampleClients.
 * SampleClients register/unregister with a SampleSource via
 * the addSampleClient/removeSampleClient methods.
 */
class SampleSource : public SampleSourceImpl {
public:
    /**
     * Add a SampleClient to this SampleSource.  The pointer
     * to the SampleClient must remain valid, until after
     * it is removed.
     */
    virtual void addSampleClient(SampleClient* c) throw() {
        addSampleClientImpl(c);
    }

    /**
     * Remove a SampleClient from this SampleSource
     */
    virtual void removeSampleClient(SampleClient* c) throw() {
        removeSampleClientImpl(c);
    }

    /**
     * How many SampleClients are currently in my list.
     */
    virtual int getClientCount() const throw() {
        return getClientCountImpl();
    }

    /**
     * Big cleanup.
     */
    virtual void removeAllSampleClients() throw() {
        removeAllSampleClientsImpl();
    }

    /**
     * How many samples have been distributed by this SampleSource.
     */
    unsigned long getNumSamplesSent() const throw() {
        return getNumSamplesSentImpl();
    }

    virtual void setNumSamplesSent(unsigned long val) throw() {
        setNumSamplesSentImpl(val);
    }

    /**
     * Distribute a sample to my clients. Calls receive() method
     * of each client, passing the pointer to the Sample.
     * Does NOT do a s->freeReference().
     */
    virtual void distribute(const Sample* s) throw()
    {
        distributeImpl(s);
    }

    /**
     * Distribute a list of samples to my clients. Calls receive() method
     * of each client, passing the pointer to the Sample.
     * Does do a s->freeReference() on each sample in the list.
     */
    virtual void distribute(const std::list<const Sample*>& samps) throw()
    {
        distributeImpl(samps);
    }

    /**
     * Request that this SampleSource flush it's buffers.
     * Default implementation passes a finish() request
     * onto all its clients.
     */
    virtual void flush() throw()
    {
        flushImpl();
    }
};
}

#endif
