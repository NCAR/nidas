// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2022, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_UTIL_MUTEXCOUNT_H
#define NIDAS_UTIL_MUTEXCOUNT_H

#include "ThreadSupport.h"

namespace nidas { namespace util {

/**
 * A class which wraps a numeric value and guards operations
 * on it with a Mutex.
 **/
template <typename T>
class MutexCount
{
public:
    explicit MutexCount(T value = 0) :
        _lock(),
        _value(value)
    {}

    T
    value()
    {
        Synchronized lock(_lock);
        return _value;
    }

    operator T()
    {
        return value();
    }

    /// Pre increment operator
    MutexCount&
    operator++()
    {
        Synchronized lock(_lock);
        ++_value;
        return *this;
    }
        
    /// Pre decrement operator
    MutexCount&
    operator--()
    {
        Synchronized lock(_lock);
        --_value;
        return *this;
    }

private:
    Mutex _lock;
    T _value;
};



}}	// namespace nidas namespace util

#endif

