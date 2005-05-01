/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef DSM_VARIABLE_H
#define DSM_VARIABLE_H

#include <DOMable.h>
#include <VariableConverter.h>
#include <Parameter.h>
#include <atdUtil/InvalidParameterException.h>

#include <string>
#include <list>

namespace dsm {

class SampleTag;

/**
 * Class describing a sampled variable.
 */
class Variable : public DOMable
{
public:

    /**
     * Create a variable.
     */
    Variable();

    /**
     * Copy constructor.
     */
    Variable(const Variable&);

    virtual ~Variable();

    /**
     * What sample am I a part of?
     */
    const SampleTag* getSampleTag() const { return sampleTag; }

    /**
     * Set the sample tag pointer.  Variable does not own the pointer.
     */
    void setSampleTag(const SampleTag* val) { sampleTag = val; }

    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    void setLongName(const std::string& val) { longname = val; }

    const std::string& getLongName() const { return longname; }

    void setUnits(const std::string& val) { units = val; }

    const std::string& getUnits() const { return units; }

    /**
     * Is this a count variable, i.e. a particle count.
     * Post-processing may need to know - typically summing
     * the counts over a time period, instead of averaging.
     */
    bool isCount() const { return iscount; }

    void setCount(bool val) { iscount = val; }

    const VariableConverter* getConverter() const { return converter; }

    /**
     * Add a parameter to this Variable. Variable
     * will then own the pointer and will delete it
     * in its destructor.
     */
    void addParameter(Parameter* val)
    {
        parameters.push_back(val);
	constParameters.push_back(val);
    }

    const std::list<const Parameter*>& getParameters() const
    {
        return constParameters;
    }

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    const SampleTag* sampleTag;

    std::string name;

    std::string longname;

    std::string units;

    bool iscount;

    VariableConverter *converter;

    /**
     * List of pointers to Parameters.
     */
    std::list<Parameter*> parameters;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> constParameters;

private:

    /**
     * Assignment not supported.
     */
    Variable& operator=(const Variable&);

};

}

#endif
