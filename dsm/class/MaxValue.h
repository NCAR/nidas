/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $

    Some C++ overloaded functions which return the maximum value
    of their integer arguments.
 ********************************************************************

*/

#ifndef DSM_MAXVALUE_H
#define DSM_MAXVALUE_H

#include <limits.h>

namespace dsm {

inline size_t maxValue(unsigned short arg)
{
    return USHRT_MAX;
}

inline size_t maxValue(short arg)
{
    return SHRT_MAX;
}

inline size_t maxValue(int arg)
{
    return INT_MAX;
}

inline size_t maxValue(unsigned int arg)
{
    return UINT_MAX;
}

inline size_t maxValue(long arg)
{
    return LONG_MAX;
}

inline size_t maxValue(unsigned long arg)
{
    return ULONG_MAX;
}

}

#endif

