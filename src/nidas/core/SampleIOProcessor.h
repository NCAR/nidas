/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_CORE_SAMPLEIOPROCESSOR_H
#define NIDAS_CORE_SAMPLEIOPROCESSOR_H


#include <nidas/core/SampleInput.h>
#include <nidas/core/SampleOutput.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/ConnectionRequester.h>
#include <nidas/core/DOMable.h>

namespace nidas { namespace core {

class DSMService;

/**
 * Interface of a processor of samples. A SampleIOProcessor reads
 * input Samples from a single SampleInput, and sends its processed
 * output Samples to one or more SampleOutputs.  
 */
class SampleIOProcessor: public SampleConnectionRequester, public DOMable
{
public:

    SampleIOProcessor();

    /**
     * Copy constructor.
     */
    SampleIOProcessor(const SampleIOProcessor&);

    virtual ~SampleIOProcessor();

    virtual SampleIOProcessor* clone() const = 0;

    virtual const std::string& getName() const;

    virtual void setName(const std::string& val);

    virtual void setOptional(bool val) 
    {
        optional = val;
    }

    virtual bool isOptional() const
    {
        return optional;
    }

    /**
     * What DSMService am I associated with?
     */
    virtual const DSMService* getService() const
    {
        return service;
    }

    virtual void setService(const DSMService* val)
    {
        service = val;
    }

    /**
     * Does this SampleIOProcessor only expect input samples from
     * one DSM?
     */
    virtual bool singleDSM() const = 0;

    /**
     * Do common operations necessary when a input has connected:
     * 1. Copy the DSMConfig information from the input to the
     *    disconnected outputs.
     * 2. Request connections for all disconnected outputs.
     *
     * connect() methods in subclasses should do whatever
     * initialization necessary before invoking this
     * SampleIOProcessor::connect().
     */
    virtual void connect(SampleInput*) throw(nidas::util::IOException);

    /**
     * Disconnect a SampleInput from this SampleIOProcessor.
     * Right now just does a flush() of all connected outputs.
     */
    virtual void disconnect(SampleInput*) throw(nidas::util::IOException);

    /**
     * Do common operations necessary when a output has connected:
     * 1. do: output->init().
     * 2. add output to a list of connected outputs.
     */
    void connected(SampleOutput* orig,SampleOutput* output) throw();

    /**
     * Do common operations necessary when a output has disconnected:
     * 1. do: output->close().
     * 2. remove output from a list of connected outputs.
     */
    void disconnected(SampleOutput* output) throw();

    /**
     * Add an SampleOutput to this SampleIOProcessor. SampleIOProcessor
     * will own the SampleOutput. Once an input has connected,
     * then SampleIOProcessor will do SampleOutput::requestConnection on
     * all these as-yet disconnected outputs.
     */
    virtual void addOutput(SampleOutput* val)
    {
	origOutputs.push_back(val);
    }

    const std::list<SampleOutput*>& getOutputs() const 
    {
        return origOutputs;
    }

    const std::set<SampleOutput*>& getConnectedOutputs() const 
    {
        return outputSet;
    }

    /**
     * Add a SampleTag to this processor.
     * Throw an exception if you don't like the variables in the sample.
     * SampleIOProcessor will own the tag.
     */
    virtual void addSampleTag(SampleTag* var)
    	throw(nidas::util::InvalidParameterException);

    virtual const std::set<const SampleTag*>& getSampleTags() const
    	{ return constSampleTags; }

    /**
     * Set the various levels of the processor identification.
     * A sensor ID is a 32-bit value comprised of four parts:
     * 6-bit not used, 10-bit DSM id, and a 16-bit processor id.
     */
    void setId(dsm_sample_id_t val) { id = SET_FULL_ID(id,val); }
    void setShortId(unsigned long val) { id = SET_SHORT_ID(id,val); }
    void setDSMId(unsigned long val) { id = SET_DSM_ID(id,val); }

    /**
     * Get the various levels of the processor's identification.
     * A sample tag ID is a 32-bit value comprised of four parts:
     * 6-bit type_id  10-bit DSM_id, and a 16-bit processor id.
     */
    dsm_sample_id_t  getId()      const { return GET_FULL_ID(id); }
    unsigned long getDSMId()   const { return GET_DSM_ID(id); }
    unsigned long getShortId() const { return GET_SHORT_ID(id); }

    SampleTagIterator getSampleTagIterator() const;

    VariableIterator getVariableIterator() const;

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

private:

    std::string name;

    std::list<SampleOutput*> origOutputs;

    /**
     * Mapping between connected outputs and the original
     * outputs.
     */
    std::map<SampleOutput*,SampleOutput*> outputMap;

    /**
     * The connected outputs, kept in a set to
     * avoid duplicates.
     */
    std::set<SampleOutput*> outputSet;

    std::list<SampleOutput*> pendingOutputClosures;

    nidas::util::Mutex outputMutex;

    /**
     * Id of this processor.  Samples from this processor will
     * have this id. It is analogous to a sensor id.
     */
    dsm_sample_id_t id;


    std::set<SampleTag*> sampleTags;

    std::set<const SampleTag*> constSampleTags;

    bool optional;

    /**
     * What service am I a part of?
     */
    const DSMService* service;
};

}}	// namespace nidas namespace core

#endif
