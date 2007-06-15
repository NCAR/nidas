/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_ISFF_GOESOUTPUT_H
#define NIDAS_DYNLD_ISFF_GOESOUTPUT_H

#include <nidas/core/SampleOutput.h>
#include <nidas/dynld/isff/GOESXmtr.h>

#include <vector>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

/**
 * A SampleOutput for packaging data for a GOES DCP Transmitter.
 */
class GOESOutput: public SampleOutputBase, public nidas::util::Runnable {

public:

    /**
     * Constructor.
     */
    GOESOutput(IOChannel* ioc = 0);

    /**
     * Copy constructor.
     */
    GOESOutput(const GOESOutput&);

    GOESOutput(const GOESOutput&,IOChannel*);

    /**
     * Destructor.
     */
    ~GOESOutput();

    void setIOChannel(IOChannel* val);

    /**
     * Clone invokes default copy constructor.
     */
    GOESOutput* clone(IOChannel* iochannel=0) const
    {
        return new GOESOutput(*this);
    }

    /**
     * The GOES transmit interval, in seconds.
     */
    int getXmitInterval() const
    { 
        if (goesXmtr) return goesXmtr->getXmitInterval();
	return -1;
    }

    /**
     * The GOES transmit offset, in seconds.
     */
    int getXmitOffset() const
    { 
        if (goesXmtr) return goesXmtr->getXmitOffset();
	return -1;
    }

    void addSampleTag(const SampleTag* tag);

    void init() throw ();

    void close() throw();

    /**
    * Raw write not supported.
    */
    size_t write(const void* buf, size_t len)
    	throw (nidas::util::IOException)
    {
	throw nidas::util::IOException(getName(),"default write","not supported");
    }

    /**
     * Send a data record to the RPC server.
    */
    bool receive(const Sample*) throw ();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    int run() throw(nidas::util::Exception);

    void interrupt();

    bool isInterrupted() const
    {
        return interrupted;
    }

    const std::list<SampleTag*>& getOutputSampleTags() const
    {
        return outputSampleTags;
    }

    void addOutputSampleTag(SampleTag* tag)
	throw(nidas::util::InvalidParameterException);


protected:
    void joinThread() throw();

    void cancelThread() throw();

private:

    GOESXmtr* goesXmtr;

    std::list<SampleTag*> outputSampleTags;

    std::list<const SampleTag*> constOutputSampleTags;

    std::map<dsm_sample_id_t,std::vector<std::vector<std::pair<int,int> > > > sampleMap;

    std::vector<SampleT<float>*> outputSamples;

    nidas::util::Mutex sampleMutex;

    nidas::util::ThreadRunnable* xmitThread;

    bool interrupted;

    int configid;

    int stationNumber;

    long long maxPeriodUsec;

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
