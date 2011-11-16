// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

class DSMConfig;
class DSMSensor;
class CalFile;
class Variable;

class VariableConverter: public DOMable
{
public:

    VariableConverter();

    /**
     * Copy constructor.
     */
    VariableConverter(const VariableConverter& x);

    /**
     * Assignment.
     */
    VariableConverter& operator=(const VariableConverter& x);

    virtual ~VariableConverter() {}

    virtual VariableConverter* clone() const = 0;

    virtual void setCalFile(CalFile*) = 0;

    virtual CalFile* getCalFile() = 0;

    virtual float convert(dsm_time_t,float v) = 0;

    void setUnits(const std::string& val) { _units = val; }

    virtual const std::string& getUnits() const { return _units; }

    void setVariable(const Variable* val) { _variable = val; }

    const Variable* getVariable() const { return _variable; }

    const DSMSensor* getDSMSensor() const;

    const DSMConfig* getDSMConfig() const;

    /**
     * Generate a string description of this VariableConverter.
     * May be used in meta-data, for example Netcdf comment.
     */
    virtual std::string toString() = 0;

    virtual void fromString(const std::string&)
    	throw(nidas::util::InvalidParameterException) {
        throw nidas::util::InvalidParameterException(
            "fromString() not supported in this VariableConverter");
    }

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
	return _constParameters;
    }

    /**
     * Fetch a parameter by name. Returns a NULL pointer if
     * no such parameter exists.
     */
    const Parameter* getParameter(const std::string& name) const;

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

protected:

    std::string _units;

    /**
     * Map of parameters by name.
     */
    std::map<std::string,Parameter*> _parameters;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> _constParameters;

    const Variable* _variable;

};

class Linear: public VariableConverter
{
public:

    Linear();

    Linear(const Linear& x);

    Linear& operator=(const Linear& x);

    Linear* clone() const;

    ~Linear();

    void setCalFile(CalFile*);

    CalFile* getCalFile();

    void setSlope(float val) { _slope = val; }

    float getSlope() const { return _slope; }

    void setIntercept(float val) { _intercept = val; }

    float getIntercept() const { return _intercept; }

    float convert(dsm_time_t t,float val);

    std::string toString();

    void fromString(const std::string&)
    	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

protected:

    dsm_time_t _calTime;

private:
    float _slope;

    float _intercept;

    CalFile* _calFile;

};

class Polynomial: public VariableConverter
{
public:

    Polynomial();

    /**
     * Copy constructor.
     */
    Polynomial(const Polynomial&);

    Polynomial& operator=(const Polynomial&);

    ~Polynomial();

    Polynomial* clone() const;

    void setCalFile(CalFile*);

    CalFile* getCalFile();

    void setCoefficients(const std::vector<float>& vals);

    const std::vector<float>& getCoefficients() const { return _coefvec; }

    float convert(dsm_time_t t,float val);

    std::string toString();

    void fromString(const std::string&)
    	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

    static float eval(float x,float *p, int np);

protected:

    dsm_time_t _calTime;

private:
    std::vector<float> _coefvec;

    float* _coefs;

    unsigned int _ncoefs;

    CalFile* _calFile;

};

/* static */
inline float Polynomial::eval(float x,float *p, int np)
{
    double y = 0.0;
    for (int i = np - 1; i > 0; i--) {
        y += p[i];
        y *= x;
    }
    y += p[0];
    return y;
}

}}	// namespace nidas namespace core

#endif
