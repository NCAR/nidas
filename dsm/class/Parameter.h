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

#ifndef DSM_PARAMETER_H
#define DSM_PARAMETER_H

#include <DOMable.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

namespace dsm {

class Parameter: public DOMable
{
public:

    enum parType {STRING_PARAM, FLOAT_PARAM, INT_PARAM, BOOL_PARAM };

    typedef enum parType parType;

    virtual ~Parameter() {}

    const std::string& getName() const { return name; }

    void setName(const std::string& val) { name = val; }

    parType getType() const { return type; }

    virtual int getLength() const = 0;

    virtual double getNumericValue(int i) const;

    virtual std::string getStringValue(int i) const;

    static Parameter* createParameter(const xercesc::DOMElement*)
        throw(atdUtil::InvalidParameterException);
                                                                                
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

    int getLength() const { return values.size(); }

    const std::vector<T> getValues() const { return values; }

    void setValues(const std::vector<T>& vals) { values = vals; }

    T getValue(int i) const { return values[i]; }

    void fromDOMElement(const xercesc::DOMElement*)
        throw(atdUtil::InvalidParameterException);
                                                                                
protected:

    /**
     * Vector of values.
     */
    std::vector<T> values;

};


}

#endif
