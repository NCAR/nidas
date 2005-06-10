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
     * Connect a SampleInput to this SampleIOProcessor.
     */
    virtual void connect(SampleInput*) throw(atdUtil::IOException) = 0;

    /**
     * Disconnect a SampleInput from this SampleIOProcessor.
     */
    virtual void disconnect(SampleInput*) throw(atdUtil::IOException) = 0;

    /**
     * Add a (disconnected) SampleOutput to this SampleIOProcessor.
     * It is the job of the SampleIOProcessor to do a requestConnection
     * on this SampleOutput, and otherwise manage the SampleOutput.
     */
    void addOutput(SampleOutput* val) { outputs.insert(val); }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
        throw(xercesc::DOMException);

    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
        throw(xercesc::DOMException);

protected:
    
    std::string name;

    std::set<SampleOutput*> outputs;
};


}

#endif
