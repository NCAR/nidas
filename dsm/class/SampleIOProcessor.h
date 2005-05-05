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
 * Interface of a processor of samples.
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

    virtual bool singleDSM() const = 0;

    virtual void connect(SampleInput*) throw(atdUtil::IOException) = 0;

    virtual void disconnect(SampleInput*) throw(atdUtil::IOException) = 0;

    void addOutput(SampleOutput* val) { outputs.push_back(val); }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
        throw(xercesc::DOMException);

    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
        throw(xercesc::DOMException);

protected:
    
    std::string name;

    // const DSMService* service;

    std::list<SampleOutput*> outputs;
};


}

#endif
