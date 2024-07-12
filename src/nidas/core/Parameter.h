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

    void assign(const Parameter&);

    virtual ~Parameter() {}

    virtual Parameter* clone() const = 0;

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

#ifdef notdef
    void setValues(const std::vector<std::string>& vals);
    void setValues(const std::vector<float>& vals);
    void setValues(const std::vector<int>& vals);
    void setValues(const std::vector<bool>& vals);
#endif

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

    virtual int getLength() const;

    virtual double getNumericValue(int i) const;

    /**
     * If this Parameter has float, int, or bool type and length 1, return the
     * value cast to bool.  Otherwise, if the context name is not empty, raise
     * an InvalidParameterException using the given context name, else return
     * false.
     */
    virtual bool getBoolValue(const std::string& name = "") const;

    virtual std::string getStringValue(int i) const;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    static Parameter*
    createParameter(const xercesc::DOMElement*, const Dictionary* d = 0);

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    virtual void
    fromDOMElement(const xercesc::DOMElement*, const Dictionary* dict) = 0;

protected:

    std::string _name;

    std::vector<std::string> _strings{};
    std::vector<float> _floats{};
    std::vector<int> _ints{};
    std::vector<bool> _bools{};

    parType _type;

    template <typename T>
    std::vector<T>& get_vector();

    template <typename T>
    void set_value(int i, const T& val);

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

#ifdef notdef
    const std::vector<T> getValues() const;

    void setValues(const std::vector<T>& vals);
#endif

    /**
     * Set ith value.
     */
    void setValue(unsigned int i, const T& val);

    /**
     * For parameters of length one, set its value.
     */
    void setValue(const T& val);

    T getValue(int i) const;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement*);

    /**
     * @throws nidas::util::InvalidParameterException;
     **/
    void fromDOMElement(const xercesc::DOMElement*, const Dictionary* dict);
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
