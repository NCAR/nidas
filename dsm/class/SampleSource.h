
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $


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
    virtual void distribute(const Sample* s)
	throw(SampleParseException,atdUtil::IOException)
    {
        distributeImpl(s);
    }

    /**
     * Distribute a list of samples to my clients. Calls receive() method
     * of each client, passing the pointer to the Sample.
     */
    virtual void distribute(const std::list<const Sample*>& samps)
	throw(SampleParseException,atdUtil::IOException)
    {
	for (std::list<const Sample*>::const_iterator si = samps.begin();
		si != samps.end(); ++si) distributeImpl(*si);
    }

};
}

#endif
