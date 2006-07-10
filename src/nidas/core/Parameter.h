/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    Some over-engineered sample classes.
 ********************************************************************

*/

#ifndef NIDAS_CORE_PARAMETER_H
#define NIDAS_CORE_PARAMETER_H

#include <nidas/core/DOMable.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

namespace nidas { namespace core {

class Parameter: public DOMable
{
public:

    enum parType {STRING_PARAM, FLOAT_PARAM, INT_PARAM, BOOL_PARAM };

    typedef enum parType parType;

    virtual void assign(const Parameter&) = 0;

    virtual ~Parameter() {}

    virtual Parameter* clone() const = 0;

    const std::string& getName() const { return name; }

    void setName(const std::string& val) { name = val; }

    parType getType() const { return type; }

    virtual int getLength() const = 0;

    virtual double getNumericValue(int i) const;

    virtual std::string getStringValue(int i) const;

    static Parameter* createParameter(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);
                                                                                
    xercesc::DOMElement*
        toDOMParent(xercesc::DOMElement* parent)
                throw(xercesc::DOMException);
                                                                                
    xercesc::DOMElement*
        toDOMElement(xercesc::DOMElement* node)
                throw(xercesc::DOMException);

protected:

    Parameter(parType t): type(t) {}

    std::string name;

    parType type;


};

/**
 * Overloaded function to return a enumerated value
 * corresponding to the type pointed to by the argument.
 */
inline Parameter::parType getParamType(std::string T)
{
    return Parameter::STRING_PARAM;
}

inline Parameter::parType getParamType(float T)
{
    return Parameter::FLOAT_PARAM;
}

inline Parameter::parType getParamType(int T)
{
    return Parameter::INT_PARAM;
}

inline Parameter::parType getParamType(bool T)
{
    return Parameter::BOOL_PARAM;
}

/**
 * A typed Parameter, with data of type T.
 */
template <class T>
class ParameterT : public Parameter {
public:

    ParameterT(): Parameter(getParamType(T())) {}

    ParameterT* clone() const;

    /**
     * A virtual assignment operator.
     */
    void assign(const Parameter& x);

    int getLength() const { return values.size(); }

    const std::vector<T> getValues() const { return values; }

    void setValues(const std::vector<T>& vals) { values = vals; }

    /**
     * Set ith value.
     */
    void setValue(unsigned int i, const T& val)
    {
	for (unsigned int j = values.size(); j < i; j++) values.push_back(T());
	if (values.size() > i) values[i] = val;
	else values.push_back(val);
    }

    /**
     * For parameters of length one, set its value.
     */
    void setValue(const T& val) {
	values.clear();
        values.push_back(val);
    }

    T getValue(int i) const { return values[i]; }

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);
                                                                                
protected:

    /**
     * Vector of values.
     */
    std::vector<T> values;

};

/**
 * Functor class for Parameter, doing an equality check of
 * parameter name and type. Can be used with find_if.
 */
class ParameterNameTypeComparator {
public:
    ParameterNameTypeComparator(const Parameter* param):p(param) {}
    bool operator()(const Parameter* x) const {
        return x->getName() == p->getName() &&
                x->getType() == p->getType();
    }
private:
    const Parameter* p;
};

}}	// namespace nidas namespace core

#endif
