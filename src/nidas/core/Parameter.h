// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/*

    A fairly generic parameter.

*/

#ifndef NIDAS_CORE_PARAMETER_H
#define NIDAS_CORE_PARAMETER_H

#include <nidas/core/DOMable.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

namespace nidas { namespace core {

class Dictionary;

class Parameter: public DOMable
{
public:

    enum parType {STRING_PARAM, FLOAT_PARAM, INT_PARAM, BOOL_PARAM };

    typedef enum parType parType;

    virtual void assign(const Parameter&) = 0;

    virtual ~Parameter() {}

    virtual Parameter* clone() const = 0;

    const std::string& getName() const { return _name; }

    void setName(const std::string& val) { _name = val; }

    parType getType() const { return _type; }

    virtual int getLength() const = 0;

    virtual double getNumericValue(int i) const;

    virtual std::string getStringValue(int i) const;

    static Parameter* createParameter(const xercesc::DOMElement*, const Dictionary* d = 0)
        throw(nidas::util::InvalidParameterException);

    virtual void fromDOMElement(const xercesc::DOMElement*, const Dictionary* dict)
        throw(nidas::util::InvalidParameterException) = 0;
                                                                                
protected:

    Parameter(parType t): _name(),_type(t) {}

    std::string _name;

    parType _type;


};

/**
 * Overloaded function to return a enumerated value
 * corresponding to the type pointed to by the argument.
 */
inline Parameter::parType getParamType(std::string)
{
    return Parameter::STRING_PARAM;
}

inline Parameter::parType getParamType(float)
{
    return Parameter::FLOAT_PARAM;
}

inline Parameter::parType getParamType(int)
{
    return Parameter::INT_PARAM;
}

inline Parameter::parType getParamType(bool)
{
    return Parameter::BOOL_PARAM;
}

/**
 * A typed Parameter, with data of type T.
 */
template <class T>
class ParameterT : public Parameter {
public:

    ParameterT(): Parameter(getParamType(T())),_values() {}

    ParameterT* clone() const;

    /**
     * A virtual assignment operator.
     */
    void assign(const Parameter& x);

    int getLength() const { return _values.size(); }

    const std::vector<T> getValues() const { return _values; }

    void setValues(const std::vector<T>& vals) { _values = vals; }

    /**
     * Set ith value.
     */
    void setValue(unsigned int i, const T& val)
    {
	for (unsigned int j = _values.size(); j < i; j++) _values.push_back(T());
	if (_values.size() > i) _values[i] = val;
	else _values.push_back(val);
    }

    /**
     * For parameters of length one, set its value.
     */
    void setValue(const T& val) {
	_values.clear();
        _values.push_back(val);
    }

    T getValue(int i) const { return _values[i]; }

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*, const Dictionary* dict)
        throw(nidas::util::InvalidParameterException);
                                                                                
protected:

    /**
     * Vector of values.
     */
    std::vector<T> _values;

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
