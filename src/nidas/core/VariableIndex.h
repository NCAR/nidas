// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
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
#ifndef NIDAS_CORE_VARIABLEINDEX_H
#define NIDAS_CORE_VARIABLEINDEX_H

namespace nidas { namespace core {

class Variable;

/**
 * A VariableIndex associates a Variable pointer with it's index in it's
 * SampleTag.  Sensor classes can use this to keep a reference to a
 * Variable and locate the Variable's data in the samples with it's
 * SampleTag.  See DSMSensor::findVariableIndex().
 **/
class VariableIndex
{
public:

    /**
     * Default constructor creates an invalid VariableIndex: the Variable
     * pointer is null and the index is -1.
     */
    VariableIndex() :
        _variable(0),
        _index(-1)
    {}

    /**
     * Construct a VariableIndex with the given Variable and index.
     */
    VariableIndex(Variable* var, int index) :
        _variable(var),
        _index(index)
    {}

    /**
     * The copy constructor copies the pointer and index of the given
     * VariableIndex.
     */
    VariableIndex(const VariableIndex& rhs) :
        _variable(rhs._variable),
        _index(rhs._index)
    {}
        
    /**
     * Assign the pointer and index of the given VariableIndex to this
     * instance.
     */
    VariableIndex& operator=(const VariableIndex& rhs)
    {
        _variable = rhs._variable;
        _index = rhs._index;
        return *this;
    }

    Variable*
    variable()
    {
        return _variable;
    }

    int
    index()
    {
        return _index;
    }

    bool
    operator!()
    {
        return _index < 0 || !_variable;
    }

    operator bool()
    {
        return _index >= 0 && _variable;
    }

private:

    Variable* _variable;
    int _index;
};

}}	// namespace nidas namespace core

#endif
