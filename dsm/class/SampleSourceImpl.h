
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$


*/

#ifndef DSM_SAMPLESOURCEIMPL_H
#define DSM_SAMPLESOURCEIMPL_H

#include <SampleClientList.h>

namespace dsm {

/**
 * Implementation for SampleSource and RawSampleSource.
 * This allows a design where an object can be both a
 * SampleSource and a RawSampleSource (because we love multiple
 * inheritance don't we :)!
 */
class SampleSourceImpl {
protected:

    SampleSourceImpl() {}

    virtual ~SampleSourceImpl() {}

    /**
     * Add a SampleClient to this SampleSourceImpl.  Users should
     * make sure that the pointer to the SampleClient remains valid,
     * until AFTER it is removed.
     */
    virtual void addSampleClientImpl(SampleClient*) throw();

    /**
     * Remove a SampleClient from this SampleSourceImpl
     */
    virtual void removeSampleClientImpl(SampleClient*) throw();

    /**
     * How many clients to I have?
     */
    virtual int getClientCountImpl() const throw();

    /**
     * Big cleanup.
     */
    virtual void removeAllSampleClientsImpl() throw();

    /**
     * How many samples have been distributed by this SampleSourceImpl.
     */
    virtual unsigned long getNumSamplesSentImpl() const
    {
	return numSamplesSent;
    }

    virtual void setNumSamplesSentImpl(unsigned long val)
    {
	numSamplesSent = val;
    }

    /**
     * Distribute this sample to my clients. Calls SampleClient::receive() method
     * of each client, passing the pointer to the Sample.
     * Does a freeReference() on the sample before returning, even
     * if a SampleClient::receive() throws an exception. If a
     * SampleClient throws an exception, the sample is not passed to
     * subsequent clients.
     */
    virtual void distributeImpl(const Sample*) throw();

    /**
     * Distribute a list of samples to my clients, calling
     * distributeImpl(const Sample*) for each sample.
     * If a SampleClient throws an exception, subsequent samples
     * in the list are not passed to SampleClients, but their
     * freeReference() method will have been called.
     */
    virtual void distributeImpl(const std::list<const Sample*>& samples)
    	throw();

protected:

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
