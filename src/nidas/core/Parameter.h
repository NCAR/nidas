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

#ifndef NIDAS_CORE_PARAMETER_H
#define NIDAS_CORE_PARAMETER_H

#include "DOMable.h"

#include <string>
#include <vector>

namespace nidas { namespace core {

class Dictionary;

/**
 * A fairly generic parameter.
 *
 * A Parameter has a name and a list of values of the same type, either
 * strings, floats, integers, or booleans.  Once the type is set, the value
 * can be changed with setValue(), whereas the assignment operator replaces
 * the current Parameter type and values.
 */
class Parameter
{
public:

    enum parType {STRING_PARAM, FLOAT_PARAM, INT_PARAM, BOOL_PARAM };

    typedef enum parType parType;

    /**
     * Create a Parameter with a name and a type.  Defaults to an empty name
     * and string type.
     */
    Parameter(const std::string& name="", parType ptype=STRING_PARAM);

    /**
     * Create a Parameter with a name and value, with a type according to the
     * type of the value.
     */
    explicit Parameter(const std::string& name, const std::string& value);
    explicit Parameter(const std::string& name, float value);
    explicit Parameter(const std::string& name, int value);
    explicit Parameter(const std::string& name, bool value);

    /**
     * Parameter with a double value is implicitly converted to FLOAT_PARAM.
     */
    explicit Parameter(const std::string& name, double value);

    virtual ~Parameter() {}

    /**
     * This allows a Parameter instance to be cloned if it was not already a
     * ParameterT subclass.  Of course, such a cloned instance cannot be
     * downcast to get the typed interface, but it could be copied or assigned
     * into a typed subclass as long as the subclass type and the type of this
     * Parameter agree.
     */
    virtual Parameter* clone() const;

    /**
     * Use default assignment and copy, so any Parameter can be replaced, and
     * things like vectors of Parameter work as expected.  If only the value
     * should change and not the name or type, then use the setValue()
     * methods.
     */
    Parameter(const Parameter&) = default;
    Parameter& operator=(const Parameter&) = default;

    const std::string& getName() const
    {
        return _name;
    }

    void setName(const std::string& val)
    {
        _name = val;
    }

    /**
     * Replace this value with the value of the Parameter @p param only if the
     * types are the same.  The name does not change.
     */
    void setValue(const Parameter& param);

    /**
     * Set ith value.
     */
    void setValue(unsigned int i, const std::string& val);
    void setValue(unsigned int i, const float& val);
    void setValue(unsigned int i, const int& val);
    void setValue(unsigned int i, const bool& val);

    /**
     * For parameters of length one, set its value.
     */
    void setValue(const std::string& val);
    void setValue(const float& val);
    void setValue(const int& val);
    void setValue(const bool& val);

    std::string getString(int i) const;
    float getFloat(int i) const;
    int getInt(int i) const;
    bool getBool(int i) const;

    parType getType() const { return _type; }

    int getLength() const;

    double getNumericValue(int i) const;

    /**
     * If this Parameter has float, int, or bool type and length 1, return the
     * value cast to bool.  Otherwise, if the context name is not empty, raise
     * an InvalidParameterException using the given context name, else return
     * false.
     */
    bool getBoolValue(const std::string& name = "") const;

    std::string getStringValue(int i) const;

    /**
     * A special case to get a string value which includes all the values,
     * concatenated as strings separated with spaces, and cache that string
     * value in the object so that c_str() is valid as long as the Parameter
     * exists.
     */
    const std::string& getStringValue() const;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    static Parameter*
    createParameter(const xercesc::DOMElement*, const Dictionary* d = 0);

    /**
     * @throws nidas::util::InvalidParameterException;
     **/
    void fromDOMElement(const xercesc::DOMElement*, const Dictionary* dict = 0);

    /**
     * If @p name matches the recognized names for parType, then set @p ptype
     * and return true.  Otherwise return false.
     */
    static bool string_to_type(const std::string& name, parType& ptype);

    /**
     * Two Parameters are equal if their name, type, and values are equal.
     */
    bool operator==(const Parameter& rhs) const;

protected:

    std::string _name;

    // this would take less memory as a union, but then we have to make sure
    // the underlying type is correctly intialized and destroyed.  a variant
    // might be nice, but not until C++14.  only one of these will ever have
    // any values, so the same dynamic allocation as with a variant.
    std::vector<std::string> _strings{};
    std::vector<float> _floats{};
    std::vector<int> _ints{};
    std::vector<bool> _bools{};

    parType _type;

    mutable std::string _cached_value{};

    template <typename T>
    std::vector<T>& get_vector();

    template <typename T>
    void set_value(int i, const T& val);

    template <typename T>
    T get_value(int i) const;

    // If type must change, all the values have to be reset.
    void set_type(parType etype);

    template <typename T>
    void
    set_from_string(const std::string& ptype, const std::string& aval);
};

/**
 * A typed Parameter, with data of type T.  This is a typed interface to
 * Parameter, where all the setValue() and setValues() methods are shadowed
 * except for the specific storage type of this subclass.  Likewise there is a
 * getValue() method which returns the storage type.
 */
template <class T>
class ParameterT : public Parameter {
public:

    ParameterT();

    ParameterT* clone() const;

    /**
     * Set ith value.
     */
    void setValue(unsigned int i, const T& val);

    /**
     * For parameters of length one, set its value.
     */
    void setValue(const T& val);

    void setValue(const Parameter& param);

    T getValue(int i) const;
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


}} // namespace nidas namespace core

#endif
