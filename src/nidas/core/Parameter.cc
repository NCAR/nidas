/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
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

#include "Parameter.h"
#include "Sample.h"	// floatNAN
#include "Dictionary.h"

#include <sstream>
#include <iostream>

using namespace std;

namespace n_u = nidas::util;

namespace nidas {
namespace core {


template <typename T>
struct get_param_type {
};

template <>
struct get_param_type<std::string> {
    static const Parameter::parType par_type{Parameter::STRING_PARAM};
};

template <>
struct get_param_type<float> {
    static const Parameter::parType par_type{Parameter::FLOAT_PARAM};
};

template <>
struct get_param_type<int> {
    static const Parameter::parType par_type{Parameter::INT_PARAM};
};

template <>
struct get_param_type<bool> {
    static const Parameter::parType par_type{Parameter::BOOL_PARAM};
};




Parameter::Parameter(const std::string& name, parType ptype):
    _name(name),
    _type(ptype)
{
}

Parameter::Parameter(const std::string& name, const std::string& value):
    _name(name),
    _type(STRING_PARAM)
{
    setValue(value);
}

Parameter::Parameter(const std::string& name, float value):
    _name(name),
    _type(FLOAT_PARAM)
{
    setValue(value);
}
Parameter::Parameter(const std::string& name, int value):
    _name(name),
    _type(INT_PARAM)
{
    setValue(value);
}

Parameter::Parameter(const std::string& name, bool value):
    _name(name),
    _type(BOOL_PARAM)
{
    setValue(value);
}




int
Parameter::getLength() const
{
    int result{0};
    switch (_type)
    {
        case FLOAT_PARAM:
            result = _floats.size();
            break;
        case INT_PARAM:
            result = _ints.size();
            break;
        case STRING_PARAM:
            result = _strings.size();
            break;
        case BOOL_PARAM:
            result = _bools.size();
            break;
    }
    return result;
}



double Parameter::getNumericValue(int i) const
{
    double result = floatNAN;
    switch (_type) {
        case FLOAT_PARAM:
            result = getFloat(i);
            break;
        case INT_PARAM:
            result = getInt(i);
            break;
        case STRING_PARAM:
            break;
        case BOOL_PARAM:
            result = getBool(i);
            break;
    }
    return result;
}

std::string Parameter::getStringValue(int i) const
{
    ostringstream ost;
    switch (_type) {
        case FLOAT_PARAM:
            ost << getFloat(i);
            break;
        case INT_PARAM:
            ost << getInt(i);
            break;
        case STRING_PARAM:
            ost << getString(i);
            break;
        case BOOL_PARAM:
            ost << getBool(i);
            break;
    }
    return ost.str();
}

bool Parameter::getBoolValue(const std::string& name) const
{
    if ((getType() != Parameter::BOOL_PARAM &&
        getType() != Parameter::INT_PARAM &&
        getType() != Parameter::FLOAT_PARAM) ||
        getLength() != 1)
    {
        if (!name.empty())
            throw n_u::InvalidParameterException(name, getName(),
                "should be a boolean or integer (FALSE=0,TRUE=1) of length 1");
        return false;
    }
    return (bool) getNumericValue(0);
}

template <>
std::vector<std::string>& Parameter::get_vector<std::string>()
{
    return _strings;
}

template <>
std::vector<float>& Parameter::get_vector<float>()
{
    return _floats;
}

template <>
std::vector<int>& Parameter::get_vector<int>()
{
    return _ints;
}

template <>
std::vector<bool>& Parameter::get_vector<bool>()
{
    return _bools;
}


#ifdef notdef
void Parameter::setValues(const std::vector<std::string>& vals)
{
    _type = STRING_PARAM;
    get_vector<std::string>() = vals;
}

void Parameter::setValues(const std::vector<float>& vals)
{
    _type = STRING_PARAM;
    get_vector<float>() = vals;
}

void Parameter::setValues(const std::vector<int>& vals)
{
    _type = STRING_PARAM;
    get_vector<int>() = vals;
}

void Parameter::setValues(const std::vector<bool>& vals)
{
    _type = STRING_PARAM;
    get_vector<bool>() = vals;
}
#endif


template <typename V, typename T>
void
set_vector_value(V& values, int i, const T& val)
{
    if (i < 0)
    {
        values.resize(1);
        i = 0;
    }
    if (i+1 > (int)values.size())
        values.resize(i+1);
    values[i] = val;
}


// Use specialization to do nothing on assignments between incompatible types.
template <>
void
set_vector_value(std::vector<std::string>&, int, const int&)
{}

template <>
void
set_vector_value(std::vector<std::string>&, int, const float&)
{}

template <>
void
set_vector_value(std::vector<std::string>&, int, const bool&)
{}

template <>
void
set_vector_value(std::vector<float>&, int, const std::string&)
{}

template <>
void
set_vector_value(std::vector<int>&, int, const std::string&)
{}

template <>
void
set_vector_value(std::vector<bool>&, int, const std::string&)
{}

template <typename T>
void
Parameter::set_value(int i, const T& val)
{
    switch (_type)
    {
        case FLOAT_PARAM:
            set_vector_value(_floats, i, val);
            break;
        case INT_PARAM:
            set_vector_value(_ints, i, val);
            break;
        case BOOL_PARAM:
            set_vector_value(_bools, i, val);
            break;
        case STRING_PARAM:
            set_vector_value(_strings, i, val);
            break;
    }
}

void Parameter::setValue(unsigned int i, const std::string& val)
{
    set_value(i, val);
}

void Parameter::setValue(unsigned int i, const float& val)
{
    set_value(i, val);
}

void Parameter::setValue(unsigned int i, const int& val)
{
    set_value(i, val);
}

void Parameter::setValue(unsigned int i, const bool& val)
{
    set_value(i, val);
}

void Parameter::setValue(const std::string& val)
{
    set_value(-1, val);
}

void Parameter::setValue(const float& val)
{
    set_value(-1, val);
}

void Parameter::setValue(const int& val)
{
    set_value(-1, val);
}

void Parameter::setValue(const bool& val)
{
    set_value(-1, val);
}

std::string Parameter::getString(int i) const
{
    return const_cast<Parameter*>(this)->get_vector<std::string>()[i];
}

float Parameter::getFloat(int i) const
{
    return const_cast<Parameter*>(this)->get_vector<float>()[i];
}

int Parameter::getInt(int i) const
{
    return const_cast<Parameter*>(this)->get_vector<int>()[i];
}

bool Parameter::getBool(int i) const
{
    return const_cast<Parameter*>(this)->get_vector<bool>()[i];
}


void Parameter::assign(const Parameter& x)
{
    // this can be implemented as normal assignment, as long as the parameter
    // types are the same.
    if (_type == x._type) {
        Parameter::operator=(x);
    }
}

#ifdef notdef
Parameter* Parameter::clone() const
{
    return new Parameter(*this);
}
#endif


Parameter* Parameter::createParameter(const xercesc::DOMElement* node,
                                      const Dictionary* dict)
{
    XDOMElement xnode(node);
    Parameter* parameter = 0;
    if(node->hasAttributes()) {
    // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            const string& aname = attr.getName();
            const string& aval = attr.getValue();
            if (aname == "type") {
                if (aval == "float")
                        parameter = new ParameterT<float>();
                else if (aval == "bool")
                        parameter = new ParameterT<bool>();
                else if (aval == "string")
                        parameter = new ParameterT<string>();
                else if (aval == "strings")
                        parameter = new ParameterT<string>();
                else if (aval == "int")
                        parameter = new ParameterT<int>();
                else if (aval == "hex")
                        parameter = new ParameterT<int>();
                else throw n_u::InvalidParameterException("parameter",
                        aname,aval);

                parameter->fromDOMElement(node,dict);

                return parameter;
            }
        }
    }
    throw n_u::InvalidParameterException("parameter",
            "element","no type attribute found");
}

template<class T>
ParameterT<T>* ParameterT<T>::clone() const
{
    return new ParameterT<T>(*this);
}


template<class T>
void ParameterT<T>::fromDOMElement(const xercesc::DOMElement* node)
{
    fromDOMElement(node,0);
}

template<class T>
void ParameterT<T>::fromDOMElement(const xercesc::DOMElement* node,
                                   const Dictionary* dict)
{

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
        // get all the attributes of the node

        const string& ptype = xnode.getAttributeValue("type");
        bool oneString = ptype == "string";

        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            const std::string& aname = attr.getName();
            std::string aval = attr.getValue();
            if (dict) aval = dict->expandString(aval);

            if (aname == "name") setName(aval);
            else if (aname == "value") {
                // get attribute value(s)

                // If type is simply "string", don't break it up.
                if (oneString) {
                    // ugly!
                    ParameterT<string>* strParam =
                            dynamic_cast<ParameterT<string>*>(this);
                    strParam->setValue(aval);
                }
                else {
                    std::istringstream ist(aval);
                    if (ptype == "hex") ist >> hex;
                    if (ptype == "bool") ist >> boolalpha;
                    T val;
                    for (int i = 0; ; i++) {
                        ist >> val;
#ifdef DEBUG
                        std::cerr << 
                            "Parameter::fromDOMElement, read, val=" << val <<
                            " eof=" << ist.eof() <<
                            " fail=" << ist.fail() << std::endl;
#endif
                        // In case a bool was entered as "0" or "1", turn off boolalpha and try again.
                        if (ist.fail()) {
                            if (ist.eof()) break;
                            ist.clear();
                            ist >> noboolalpha >> val;
                            if (ist.fail())
                                throw n_u::InvalidParameterException(
                                    "parameter",getName(),aval);
                            ist >> boolalpha;
                        }
                        setValue(i,val);
                    }
                }
#ifdef DEBUG
                std::cerr << "Parameter::fromDOMElement, getLength()=" <<
                        getLength() << std::endl;
#endif
            }
            else if (aname != "type" && aname != "xmlns")
                // XMLConfigWriter seems to add xmlns attributes
                throw n_u::InvalidParameterException(
                    "parameter",aname,aval);
        }
    }
}


template <class T>
ParameterT<T>::ParameterT():
    Parameter("", get_param_type<T>::par_type)
{
}


#ifdef notdef
template <class T>
const std::vector<T> ParameterT<T>::getValues() const
{
    return const_cast<ParameterT<T>*>(this)->get_vector<T>();
}

template <class T>
void ParameterT<T>::setValues(const std::vector<T>& vals) {
    get_vector<T>() = vals;
}
#endif

template <class T>
void ParameterT<T>::setValue(unsigned int i, const T& val)
{
    set_value(i, val);
}

template <class T>
void ParameterT<T>::setValue(const T& val) {
    set_value(-1, val);
}

template <class T>
T ParameterT<T>::getValue(int i) const
{
    return const_cast<ParameterT<T>*>(this)->get_vector<T>()[i];
}


template class ParameterT<float>;
template class ParameterT<int>;
template class ParameterT<string>;
template class ParameterT<bool>;

} // namespace core
} // namespace nidas
