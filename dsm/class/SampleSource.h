
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
 * A source of samples.
 */
class SampleSource : public SampleSourceImpl {
public:
    /**
     * Add a SampleClient to this SampleSource.  The pointer
     * to the SampleClient must remain valid, until after
     * it is removed.
     */
    void addSampleClient(SampleClient* c) throw() {
        addSampleClientImpl(c);
    }

    /**
     * Remove a SampleClient from this SampleSource
     */
    void removeSampleClient(SampleClient* c) throw() {
        removeSampleClientImpl(c);
    }

    /**
     * Big cleanup.
     */
    void removeAllSampleClients() throw() {
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
     * Afterwards does a freeReference() on the sample.
     */
    virtual void distribute(const Sample* s) throw()
    {
        distributeImpl(s);
    }

    /**
     * Distribute a list of samples to my clients. Calls receive() method
     * of each client, passing the pointer to the Sample.
     */
    virtual void distribute(const std::list<const Sample*>& samps) throw()
    {
        distributeImpl(samps);
    }

};
}

#endif
