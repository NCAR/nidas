/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef NIDAS_CORE_VARIABLE_H
#define NIDAS_CORE_VARIABLE_H

#include <nidas/core/DOMable.h>
#include <nidas/core/VariableConverter.h>
#include <nidas/core/Parameter.h>
#include <nidas/util/InvalidParameterException.h>

#include <string>
#include <list>

namespace nidas { namespace core {

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
     * siteSuffix may be empty strings.  The suffix commonly
     * comes from the DSMSensor::getFullSuffix(), containing
     * an optional sensor suffix and a height/depth string.
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
     * Try to determine the associated site for this variable.
     * A reference to the Site is not kept with the variable.
     * Instead this method uses the station number, getStation(),
     * to find a site with the given number, or if that fails,
     * uses SampleTag::getSite().
     */
    const Site* getSite() const;

    /**
     * Set the full name. This clears the suffix and site
     * portions of the name.  Once this is called,
     * the suffix and siteSuffix fields are cleared.
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
     * @return -1: the wild card value, matching any site,
     *          0: the "non" site, or the project-wide site,
     *        1-N: a site number
     */
    int getStation() const { return station; }

    void setStation(int val) { station = val; }

    void setSiteAttributes(const Site* site);

    /**
     * Set the VariableConverter for this Variable.
     * Variable will own the pointer and will delete it.
     */
    void setConverter(VariableConverter* val) {
	delete converter;
    	converter = val;
    }

    VariableConverter* getConverter() const { return converter; }

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

    void setMissingValue(float val)
    {
        missingValue = val;
    }

    float getMissingValue() const
    {
        return missingValue;
    }

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

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

    float missingValue;

};

}}	// namespace nidas namespace core

#endif
