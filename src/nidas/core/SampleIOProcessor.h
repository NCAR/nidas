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


#include <nidas/core/ConnectionRequester.h>
#include <nidas/core/SampleSource.h>
#include <nidas/core/SampleSourceSupport.h>
#include <nidas/core/DOMable.h>
#include <nidas/core/Sample.h>  // dsm_sample_id_t

namespace nidas { namespace core {

class DSMService;
class SampleOutput;
class SampleTag;
class Parameter;

/**
 * Interface of a processor of samples. A SampleIOProcessor reads
 * input Samples from a SamplePipeline, and sends its processed
 * output Samples to one or more SampleOutputs.  
 */
class SampleIOProcessor: public SampleSource, public SampleConnectionRequester,  public DOMable
{
public:

    /**
     * Does this processor generate raw or processed samples.
     * Typically they are processed samples (duh), but not
     * always.
     */
    SampleIOProcessor(bool raw);

    virtual ~SampleIOProcessor();

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

    dsm_sample_id_t  getId()      const { return GET_FULL_ID(_id); }

    void setSampleId(int val) { _id = SET_SPS_ID(_id,val); }

    unsigned int getSampleId()      const { return GET_SPS_ID(_id); }

    /**
     * Connect a SampleSource to this SampleIOProcessor. SampleIOProcessor
     * does not own the SampleSource.
     */
    virtual void connect(SampleSource*)
        throw(nidas::util::InvalidParameterException,nidas::util::IOException) = 0;

    /**
     * Disconnect a SampleSource from this SampleIOProcessor.
     */
    virtual void disconnect(SampleSource*) throw() = 0;


    SampleSource* getRawSampleSource()
    {
        return 0;
    }

    SampleSource* getProcessedSampleSource()
    {
        return &_source;
    }

    /**
     * Add a request for a SampleTag from this SampleIOProcessor.
     * This SampleIOProcessor will own the SampleTag.
     */
    virtual void addRequestedSampleTag(SampleTag* tag)
	throw(nidas::util::InvalidParameterException);

    virtual std::list<const SampleTag*> getRequestedSampleTags() const;

    /**
     * Implementation of SampleSource::addSampleTag().
     */
    void addSampleTag(const SampleTag* tag) throw();

    void removeSampleTag(const SampleTag* tag) throw();

    /**
     * Implementation of SampleSource::getSampleTags().
     */
    std::list<const SampleTag*> getSampleTags() const
    {
        return _source.getSampleTags();
    }

    /**
     * Implementation of SampleSource::getSampleTagIterator().
     */
    SampleTagIterator getSampleTagIterator() const
    {
        return _source.getSampleTagIterator();
    }

    /**
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClient(SampleClient* client) throw()
    {
        _source.addSampleClient(client);
    }

    void removeSampleClient(SampleClient* client) throw()
    {
        _source.removeSampleClient(client);
    }

    /**
     * Add a Client for a given SampleTag.
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClientForTag(SampleClient* client,const SampleTag* tag) throw()
    {
        _source.addSampleClientForTag(client,tag);
    }

    void removeSampleClientForTag(SampleClient* client,const SampleTag* tag) throw()
    {
        _source.removeSampleClientForTag(client,tag);
    }

    int getClientCount() const throw()
    {
        return _source.getClientCount();
    }

    void flush() throw()
    {
        return _source.flush();
    }


    const SampleStats& getSampleStats() const
    {
        return _source.getSampleStats();
    }

    /**
     * Add an SampleOutput to this SampleIOProcessor. This is used
     * to add a desired SampleOutput to this SampleIOProcessor.
     * SampleIOProcessor will own the SampleOutput. Once a SampleSource
     * has connected, then SampleIOProcessor is responsible for
     * do SampleOutput::requestConnection, or
     * SampleOutputRequestThread::addConnectRequest()
     * on all these as-yet disconnected outputs.
     */
    virtual void addOutput(SampleOutput* val)
    {
	_origOutputs.push_back(val);
    }

    virtual const std::list<SampleOutput*>& getOutputs() const 
    {
        return _origOutputs;
    }

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

    virtual void printStatus(std::ostream&,float deltat,int&) throw() {}

protected:

    SampleSourceSupport _source;

    mutable nidas::util::Mutex _tagsMutex;

    std::list<SampleTag*> _requestedTags;

private:

    std::string _name;

    dsm_sample_id_t  _id;

    std::list<SampleOutput*> _origOutputs;

    bool _optional;

    /**
     * What service am I a part of?
     */
    const DSMService* _service;

    std::list<Parameter*> _parameters;

    std::list<const Parameter*> _constParameters;

    std::list<const SampleTag*> _constRequestedTags;

    /**
     * Copy not supported.
     */
    SampleIOProcessor(const SampleIOProcessor&);

    /**
     * Assignment not supported.
     */
    SampleIOProcessor& operator=(const SampleIOProcessor&);

};

}}	// namespace nidas namespace core

#endif
