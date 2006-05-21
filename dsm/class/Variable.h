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
class Site;

/**
 * Class describing a sampled variable.
 */
class Variable : public DOMable
{
public:

    typedef enum var_type { CONTINUOUS, COUNTER, CLOCK, OTHER, WEIGHT } type_t;

    /**
     * Create a variable.
     */
    Variable();

    /**
     * Copy constructor.
     */
    Variable(const Variable&);

    /**
     * Assignment.
     */
    Variable& operator=(const Variable&);

    virtual ~Variable();

    /**
     * Equivalence operator for Variable, checks
     * length, name and station equivalence.
     */
    bool operator == (const Variable& x) const;

    bool operator != (const Variable& x) const;

    bool operator < (const Variable& x) const;

    type_t getType() const { return type; }

    void setType(type_t val) { type = val; }

    /**
     * What sample am I a part of?
     */
    const SampleTag* getSampleTag() const { return sampleTag; }

    /**
     * Set the sample tag pointer.  Variable does not own the pointer.
     */
    void setSampleTag(const SampleTag* val) { sampleTag = val; }

    /**
     * Convenience routine to get the SampleTag rate.
     */
    float getSampleRate() const;

    /**
     * Set the name prefix.  The full variable name will
     * be the concatenation of: prefix + suffix + site
     */
    void setPrefix(const std::string& val)
    {
        prefix = val;
	name = prefix + suffix + siteSuffix;
	nameWithoutSite = prefix + suffix;
    }

    const std::string& getPrefix() const { return prefix; }

    /**
     * Variable suffix, which is added to the name.  The full variable name
     * is created from  prefix + suffix + siteSuffix.  suffix and/or
     * siteSuffix may be empty strings.
     */
    const std::string& getSuffix() const { return suffix; }

    void setSuffix(const std::string& val)
    {
        suffix = val;
	name = prefix + suffix + siteSuffix;
	nameWithoutSite = prefix + suffix;
    }

    /**
     * Site suffix, which is added to the name.
     */
    const std::string& getSiteSuffix() const { return siteSuffix; }

    void setSiteSuffix(const std::string& val);

    /**
     * Set the full name. This clears the suffix and site
     * portions of the name.
     */
    void setName(const std::string& val) 
    {
	suffix.clear();
	siteSuffix.clear();
	prefix = val;
        name = prefix;
	nameWithoutSite = prefix;
    }

    const std::string& getName() const { return name; }

    /**
     * Get the name without the site suffix.
     */
    const std::string& getNameWithoutSite() const { return nameWithoutSite; }

    /**
     * Descriptive, long name, e.g. "Ambient air temperature".
     */
    void setLongName(const std::string& val) { longname = val; }

    const std::string& getLongName() const { return longname; }

    void setUnits(const std::string& val) { units = val; }

    const std::string& getUnits() const { return units; }

    /**
     * How many values in this variable?
     */
    size_t getLength() const { return length; }

    void setLength(size_t val) { length = val; }

    /**
     * Station number of this variable:
     * @return -1: not associated with a specific site number
     *        0-N: a site number
     */
    int getStation() const { return station; }

    void setStation(int val);

    void setSite(const Site* val);

    /**
     * Set the VariableConverter for this Variable.
     * Variable will own the pointer and will delete it.
     */
    void setConverter(VariableConverter* val) {
	delete converter;
    	converter = val;
    }

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

private:

    const SampleTag* sampleTag;

    std::string name;

    std::string nameWithoutSite;

    std::string prefix;

    std::string suffix;

    std::string siteSuffix;

    int station;

    std::string longname;

    std::string units;

    type_t type;

    size_t length;

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

};

}

#endif
