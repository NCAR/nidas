/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_CORE_VARIABLECONVERTER_H
#define NIDAS_CORE_VARIABLECONVERTER_H

#include <nidas/core/DOMable.h>
#include <nidas/core/Sample.h>
#include <nidas/core/Parameter.h>

#include <string>
#include <vector>

namespace nidas { namespace core {

class CalFile;

class VariableConverter: public DOMable
{
public:

    virtual ~VariableConverter() {}

    virtual VariableConverter* clone() const = 0;

    virtual void setCalFile(CalFile*) = 0;

    virtual CalFile* getCalFile() = 0;

    virtual float convert(dsm_time_t,float v) = 0;

    void setUnits(const std::string& val) { units = val; }
    virtual const std::string& getUnits() const { return units; }

    virtual std::string toString() const = 0;

    virtual void fromString(const std::string&)
    	throw(nidas::util::InvalidParameterException) = 0;

    static VariableConverter* createVariableConverter(
    	XDOMElement& child);

    static VariableConverter* createFromString(const std::string&)
    	throw(nidas::util::InvalidParameterException);

    /**
     * Add a parameter to this VariableConverter. VariableConverter
     * will then own the pointer and will delete it
     * in its destructor. If a Parameter exists with the
     * same name, it will be replaced with the new Parameter.
     */
    void addParameter(Parameter* val);

    /**
     * Get list of parameters.
     */
    const std::list<const Parameter*>& getParameters() const
    {
	return constParameters;
    }

    /**
     * Fetch a parameter by name. Returns a NULL pointer if
     * no such parameter exists.
     */
    const Parameter* getParameter(const std::string& name) const;

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);


protected:

    std::string units;

    /**
     * Map of parameters by name.
     */
    std::map<std::string,Parameter*> parameters;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> constParameters;

};

class Linear: public VariableConverter
{
public:

    Linear();

    Linear(const Linear& x);

    Linear* clone() const;

    ~Linear();

    void setCalFile(CalFile*);

    CalFile* getCalFile();

    void setSlope(float val) { slope = val; }

    float getSlope() const { return slope; }

    void setIntercept(float val) { intercept = val; }

    float getIntercept() const { return intercept; }

    float convert(dsm_time_t t,float val);

    std::string toString() const;

    void fromString(const std::string&)
    	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

private:
    float slope;

    float intercept;

    CalFile* calFile;

    dsm_time_t calTime;


};

class Polynomial: public VariableConverter
{
public:

    Polynomial();

    /**
     * Copy constructor.
     */
    Polynomial(const Polynomial&);

    ~Polynomial();

    Polynomial* clone() const;

    void setCalFile(CalFile*);

    CalFile* getCalFile();

    void setCoefficients(const std::vector<float>& vals);

    const std::vector<float>& getCoefficients() const { return coefvec; }

    float convert(dsm_time_t t,float val);

    std::string toString() const;

    void fromString(const std::string&)
    	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

private:
    std::vector<float> coefvec;

    float* coefs;

    unsigned int ncoefs;

    CalFile* calFile;

    dsm_time_t calTime;

};

}}	// namespace nidas namespace core

#endif
