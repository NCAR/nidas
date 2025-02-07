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
using nidas::util::InvalidParameterException;


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

Parameter::Parameter(const std::string& name, double value):
    _name(name),
    _type(FLOAT_PARAM)
{
    setValue((float)value);
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


Parameter*
Parameter::clone() const
{
    return new Parameter(*this);
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


bool
Parameter::check(parType ptype, int len) const
{
    return _type == ptype && getLength() == len;
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
            return getString(i);
        case BOOL_PARAM:
            ost << getBool(i);
            break;
    }
    return ost.str();
}


const std::string&
Parameter::getStringValue() const
{
    ostringstream ost;
    for (int i = 0; i < getLength(); ++i)
    {
        ost << (i > 0 ? " " : "");
        ost << getStringValue(i);
    }
    _cached_value = ost.str();
    return _cached_value;
}


bool
Parameter::get(float& value) const
{
    if (check(FLOAT_PARAM, 1))
    {
        value = getFloat(0);
        return true;
    }
    return false;
}


bool
Parameter::get(int& value) const
{
    if (check(INT_PARAM, 1))
    {
        value = getInt(0);
        return true;
    }
    return false;
}


bool Parameter::getBoolValue(const std::string& name) const
{
    if ((getType() != Parameter::BOOL_PARAM &&
        getType() != Parameter::INT_PARAM &&
        getType() != Parameter::FLOAT_PARAM) ||
        getLength() != 1)
    {
        if (!name.empty())
            throw InvalidParameterException(name, getName(),
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


template <typename V, typename T>
void
safe_get(T& val, V& v, int i)
{
    if (i < (int)v.size())
        val = v[i];
}


template <typename T>
T
Parameter::get_value(int i) const
{
    T val{};
    switch (_type)
    {
        case FLOAT_PARAM:
            safe_get(val, _floats, i);
            break;
        case INT_PARAM:
            safe_get(val, _ints, i);
            break;
        case BOOL_PARAM:
            safe_get(val, _bools, i);
            break;
        case STRING_PARAM:
            break;
    }
    return val;
}


template <>
std::string
Parameter::get_value(int i) const
{
    std::string v;
    switch (_type)
    {
        case STRING_PARAM:
            safe_get(v, _strings, i);
            break;
        default:
            break;
    }
    return v;
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
    return get_value<std::string>(i);
}

float Parameter::getFloat(int i) const
{
    return get_value<float>(i);
}

int Parameter::getInt(int i) const
{
    return get_value<int>(i);
}

bool Parameter::getBool(int i) const
{
    return get_value<bool>(i);
}


void Parameter::setValue(const Parameter& x)
{
    // like normal assignment, as long as the parameter types are the same,
    // but without changing the name.
    if (_type == x._type) {
        _floats = x._floats;
        _ints = x._ints;
        _bools = x._bools;
        _strings = x._strings;
    }
}


bool
Parameter::string_to_type(const std::string& name, parType& ptype)
{
    static const std::map<std::string, parType> type_names {
        {"float", FLOAT_PARAM},
        {"bool", BOOL_PARAM},
        {"string", STRING_PARAM},
        {"strings", STRING_PARAM},
        {"int", INT_PARAM},
        {"hex", INT_PARAM}
    };
    bool found{ false };

    auto it = type_names.find(name);
    if (it != type_names.end() && (found = true))
        ptype = it->second;
    return found;
}


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
                else throw InvalidParameterException("parameter",
                        aname,aval);

                parameter->fromDOMElement(node,dict);

                return parameter;
            }
        }
    }
    throw InvalidParameterException("parameter",
            "element","no type attribute found");
}

template<class T>
ParameterT<T>* ParameterT<T>::clone() const
{
    return new ParameterT<T>(*this);
}


void Parameter::set_type(parType etype)
{
    _type = etype;
    _floats.clear();
    _ints.clear();
    _bools.clear();
    _strings.clear();
}


template <typename T>
void
Parameter::set_from_string(const std::string& ptype, const std::string& aval)
{
    // If type is simply "string", don't break it up.
    if (ptype == "string") {
        setValue(aval);
    }
    else {
        std::istringstream ist(aval);
        if (ptype == "hex") ist >> hex;
        if (ptype == "bool") ist >> boolalpha;
        T val;
        for (int i = 0; ; i++) {
            ist >> val;
            // In case a bool was entered as "0" or "1", turn off boolalpha and try again.
            if (ist.fail()) {
                if (ist.eof()) break;
                ist.clear();
                ist >> noboolalpha >> val;
                if (ist.fail())
                    throw InvalidParameterException(
                        "parameter",getName(),aval);
                ist >> boolalpha;
            }
            setValue(i, val);
        }
    }
}


void Parameter::fromDOMElement(const xercesc::DOMElement* node,
                               const Dictionary* dict)
{
    XDOMElement xnode(node);
    if(!node->hasAttributes())
    {
        return;
    }
    // get all the attributes of the node
    const string& ptype = xnode.getAttributeValue("type");

    // make sure type is consistent, in case not set by a subclass.
    parType etype;
    if (! string_to_type(ptype, etype))
        throw InvalidParameterException("parameter", "type",
                                        ptype + ": not a recognized parameter type");
    set_type(etype);

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
            switch (_type)
            {
                case FLOAT_PARAM:
                    set_from_string<float>(ptype, aval);
                    break;
                case INT_PARAM:
                    set_from_string<int>(ptype, aval);
                    break;
                case STRING_PARAM:
                    set_from_string<std::string>(ptype, aval);
                    break;
                case BOOL_PARAM:
                    set_from_string<bool>(ptype, aval);
                    break;
            }
        }
        else if (aname != "type" && !DOMable::ignoredAttribute(aname)) {
            throw InvalidParameterException("parameter", aname, aval);
        }
    }
}


bool Parameter::operator==(const Parameter& rhs) const
{
    // only one of the vector values will contain anything, according to the
    // type, so no harm in comparing all of them.
    return _name == rhs._name &&
        _type == rhs._type &&
        _strings == rhs._strings &&
        _floats == rhs._floats &&
        _ints == rhs._ints &&
        _bools == rhs._bools;
}


template <class T>
ParameterT<T>::ParameterT():
    Parameter("", get_param_type<T>::par_type)
{
}


template <class T>
void ParameterT<T>::setValue(unsigned int i, const T& val)
{
    Parameter::setValue(i, val);
}

template <class T>
void ParameterT<T>::setValue(const T& val)
{
    Parameter::setValue(val);
}

template <class T>
void ParameterT<T>::setValue(const Parameter& param)
{
    Parameter::setValue(param);
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
