/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef DSM_SAMPLEIOPROCESSOR_H
#define DSM_SAMPLEIOPROCESSOR_H


#include <SampleInput.h>
#include <SampleOutput.h>
#include <ConnectionRequester.h>
#include <DOMable.h>

namespace dsm {

class DSMConfig;
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
    virtual void connect(SampleInput*) throw(atdUtil::IOException);

    /**
     * Disconnect a SampleInput from this SampleIOProcessor.
     * Right now just does a flush() of all connected outputs.
     */
    virtual void disconnect(SampleInput*) throw(atdUtil::IOException);

    /**
     * Do common operations necessary when a output has connected:
     * 1. do: output->init().
     * 2. add output to a list of connected outputs.
     */
    virtual void connected(SampleOutput* output) throw();

    /**
     * Do common operations necessary when a output has disconnected:
     * 1. do: output->close().
     * 2. remove output from a list of connected outputs.
     */
    virtual void disconnected(SampleOutput* output) throw();

    /**
     * Add an SampleOutput to this SampleIOProcessor. SampleIOProcessor
     * will own the SampleOutput. Once an input has connected,
     * then SampleIOProcessor will do SampleOutput::requestConnection on
     * all these as-yet disconnected outputs.
     */
    virtual void addDisconnectedOutput(SampleOutput* val)
    {
	dconOutputs.push_back(val);
    }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
        throw(xercesc::DOMException);

    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
        throw(xercesc::DOMException);

protected:

    virtual void addConnectedOutput(SampleOutput* val);

    virtual void removeConnectedOutput(SampleOutput* val);

    std::string name;

    std::list<SampleOutput*> dconOutputs;

    std::list<SampleOutput*> conOutputs;

};


}

#endif
