/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $Revision: 3648 $

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/util/RunningAverage.h $
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
