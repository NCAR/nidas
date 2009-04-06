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
        _optional = val;
    }

    virtual bool isOptional() const
    {
        return _optional;
    }

    /**
     * What DSMService am I associated with?
     */
    virtual const DSMService* getService() const
    {
        return _service;
    }

    virtual void setService(const DSMService* val)
    {
        _service = val;
    }

    /**
     * Should this Processor be cloned on each connection?
     */
    virtual bool cloneOnConnection() const = 0;

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
    virtual void disconnect(SampleInput*) throw();

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
	_origOutputs.push_back(val);
    }

    const std::list<SampleOutput*>& getOutputs() const 
    {
        return _origOutputs;
    }

    const std::set<SampleOutput*>& getConnectedOutputs() const 
    {
        return _outputSet;
    }

    /**
     * Add a SampleTag to this processor.
     * Throw an exception if you don't like the variables in the sample.
     * SampleIOProcessor will own the tag.
     */
    virtual void addSampleTag(SampleTag* var)
    	throw(nidas::util::InvalidParameterException);

    virtual const std::list<const SampleTag*>& getSampleTags() const
    	{ return _constSampleTags; }

    /**
     * Set the various levels of the processor identification.
     * A sensor ID is a 32-bit value comprised of four parts:
     * 6-bit not used, 10-bit DSM id, and a 16-bit processor id.
     */
    void setId(dsm_sample_id_t val) { _id = SET_FULL_ID(_id,val); }
    void setShortId(unsigned int val) { _id = SET_SHORT_ID(_id,val); }
    void setDSMId(unsigned int val) { _id = SET_DSM_ID(_id,val); }

    /**
     * Get the various levels of the processor's identification.
     * A sample tag ID is a 32-bit value comprised of four parts:
     * 6-bit type_id  10-bit DSM_id, and a 16-bit processor id.
     */
    dsm_sample_id_t  getId()      const { return GET_FULL_ID(_id); }
    unsigned int getDSMId()   const { return GET_DSM_ID(_id); }
    unsigned int getShortId() const { return GET_SHORT_ID(_id); }

    SampleTagIterator getSampleTagIterator() const;

    VariableIterator getVariableIterator() const;

    /**
     * Add a parameter to this SampleIOProcessor, which
     * will then own the pointer and will delete it
     * in its destructor. If a Parameter exists with the
     * same name, it will be replaced with the new Parameter.
     */
    void addParameter(Parameter* val)
	throw(nidas::util::InvalidParameterException);

    /**
     * Get list of parameters.
     */
    const std::list<const Parameter*>& getParameters() const
    {
	return _constParameters;
    }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    virtual void printStatus(std::ostream&,float deltat,const char* rowStripe) throw() {}

protected:
    /**
     * Mapping between connected outputs and the original
     * outputs.
     */
    std::map<SampleOutput*,SampleOutput*> _outputMap;

private:

    std::string _name;

    std::list<SampleOutput*> _origOutputs;

    /**
     * The connected outputs, kept in a set to
     * avoid duplicates.
     */
    std::set<SampleOutput*> _outputSet;

    std::list<SampleOutput*> _pendingOutputClosures;

    nidas::util::Mutex _outputMutex;

    /**
     * Id of this processor.  Samples from this processor will
     * have this id. It is analogous to a sensor id.
     */
    dsm_sample_id_t _id;


    std::list<SampleTag*> _sampleTags;

    std::list<const SampleTag*> _constSampleTags;

    bool _optional;

    /**
     * What service am I a part of?
     */
    const DSMService* _service;

    std::list<Parameter*> _parameters;

    std::list<const Parameter*> _constParameters;
};

}}	// namespace nidas namespace core

#endif
