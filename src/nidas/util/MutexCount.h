// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2011-11-16 15:03:17 -0700 (Wed, 16 Nov 2011) $

    $LastChangedRevision: 6326 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/util/MutexCount.h $

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

