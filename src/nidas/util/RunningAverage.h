// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $Revision$

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef _nidis_util_RunningAverage_h_
#define _nidis_util_RunningAverage_h_

#include <cstring>

namespace nidas { namespace util {

/**
 * Basic running average template class.  The output is the average of the
 * previous @param i inputs.
 */
template<class T, int i> class RunningAverage
{
public:
  RunningAverage() : _size(i), _sum(0), _nValues(0), _index(0)
    { memset((char *)_values, 0, sizeof(_values)); }

  T average(T newValue)
    {
    _sum -= _values[_index];
    _values[_index++] = newValue;
    _sum += newValue;

    if (_nValues < _size)
      ++_nValues;

    if (_index >= _size)
      _index = 0;

    return (T)(_sum / _nValues);
    }

private:
  T _values[i];
  unsigned int _size;

  double _sum;

  unsigned int _nValues;
  unsigned int _index;
};

} }

#endif
