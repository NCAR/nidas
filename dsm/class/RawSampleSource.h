
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $


*/

#ifndef DSM_RAWSAMPLESOURCE_H
#define DSM_RAWSAMPLESOURCE_H

#include <SampleSource.h>

namespace dsm {

/**
 * Wrapper around a SampleSourceImpl.  Allows a DSMSensor
 * to be both a SampleSource and a RawSampleSource (because we
 * love multiple inheritance don't we :)!
 */
class RawSampleSource: SampleSourceImpl {

public:
    /**
     * Add a SampleClient to this SampleSource. 
     * If the SampleClient is already a client, it is
     * first removed, then added, so that the receive method
     * of clients are not called multiple times for each sample.
     * The pointer to the SampleClient must remain valid, until after
     * it is removed.
     */
    void addRawSampleClient(SampleClient* c) throw() {
        addSampleClientImpl(c);
    }

    /**
     * Remove a SampleClient from this SampleSource
     */
    void removeRawSampleClient(SampleClient* c) throw() {
        removeSampleClientImpl(c);
    }

    /**
     * Big cleanup.
     */
    void removeAllRawSampleClients() throw() {
        removeAllSampleClientsImpl();
    }

    /**
     * How many samples have been distributed by this SampleSource.
     */
    unsigned long getNumRawSamplesSent() const throw() {
        return getNumSamplesSentImpl();
    }

    virtual void setNumRawSamplesSent(unsigned long val) throw() {
        setNumSamplesSentImpl(val);
    }

    /**
     * Distribute a sample to my clients. Calls receive() method
     * of each client, passing the pointer to the Sample.
     */
    virtual void distributeRaw(const Sample* s)
	throw(SampleParseException,atdUtil::IOException)
    {
        distributeImpl(s);
    }


};
}

#endif
