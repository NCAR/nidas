/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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

#include "Sample.h"

#include <cmath>
#include <iostream>
#include <cstring> // std::memcpy(), strcpy(), strlen()
#include <limits> // std::numeric_limits<T>

const float nidas::core::floatNAN = std::nanf("");

const double nidas::core::doubleNAN = std::nan("");

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

#ifdef MUTEX_PROTECT_REF_COUNTS
/* static */
// n_u::Mutex Sample::refLock;
#endif

#ifndef PROTECT_NSAMPLES
/* static */
int Sample::_nsamps(0);
#else
/* static */
n_u::MutexCount<int> Sample::_nsamps(0);
#endif

#if defined(ENABLE_VALGRIND) && !defined(PROTECT_NSAMPLES)

#include <valgrind/helgrind.h>

class Sample::InitValgrind
{
public:
  InitValgrind()
  {
    VALGRIND_HG_DISABLE_CHECKING(&Sample::_nsamps, sizeof(Sample::_nsamps));
  }
};

static Sample::InitValgrind ig;

#endif


unsigned int SampleHeader::getMaxDataLength()
{
    return std::numeric_limits<dsm_sample_length_t>::max();
}


/**
 * Allocate data.
 * @param val: number of DataT's to allocated.
 * 
 * @throws SampleLengthException if the number of bytes for @p val DataT's
 * is larger than the maximum value of dsm_sample_length_t.
 */
template <typename DataT>
void SampleT<DataT>::allocateData(unsigned int val)
{
    if (val > getMaxDataLength())
        throw SampleLengthException(
            "SampleT::allocateData:", val, getMaxDataLength());
    if (_allocLen < val * sizeof(DataT))
    {
        delete [] _data;
        _data = new DataT[val];
        _allocLen = val * sizeof(DataT);
        setDataLength(0);
    }
}

/**
 * Re-allocate data, space, keeping contents.
 * @param val: number of DataT's to allocated.
 * 
 * @throws SampleLengthException if the number of bytes for @p val DataT's
 * is larger than the maximum value of dsm_sample_length_t.
 */
template <typename DataT>
void SampleT<DataT>::reallocateData(unsigned int val)
{
    if (val > getMaxDataLength())
        throw SampleLengthException(
            "SampleT::reallocateData:", val, getMaxDataLength());
    if (_allocLen < val * sizeof(DataT))
    {
        DataT* newdata = new DataT[val];
        std::memcpy(newdata,_data,_allocLen);
        delete [] _data;
        _data = newdata;
        _allocLen = val * sizeof(DataT);
    }
}

template <typename DataT>
void SampleT<DataT>::setValues(std::initializer_list<DataT> values)
{
    unsigned int len = values.size();
    allocateData(len);
    setDataLength(len);
    unsigned int i = 0;
    for (auto v : values)
    {
        _data[i++] = v;
    }
}

template <typename DataT>
SampleT<DataT>::SampleT(std::initializer_list<DataT> values) :
    Sample(sample_type_traits<DataT>::sample_type_enum),
    _data(0),_allocLen(0)
{
    setValues(values);
}

template <typename DataT>
SampleT<DataT>::~SampleT()
{
    delete [] _data;
}

template <typename DataT>
sampleType SampleT<DataT>:: getType() const
{
    return getSampleType(_data);
}

template <typename DataT>
unsigned int SampleT<DataT>::getDataLength() const
{
    return getDataByteLength() / sizeof(DataT);
}

template <typename DataT>
void SampleT<DataT>::setDataLength(unsigned int val)
{
    if (val > getAllocLength())
        throw SampleLengthException(
            "SampleT::setDataLength:", val, getAllocLength());
    _header.setDataByteLength(val * sizeof(DataT));
}

template <typename DataT>
unsigned int SampleT<DataT>::getMaxDataLength()
{
    return SampleHeader::getMaxDataLength() / sizeof(DataT);
}

// Explicitly instantiate SampleT implementations for all the supported types.
template class nidas::core::SampleT<char>;
template class nidas::core::SampleT<unsigned char>;
template class nidas::core::SampleT<short>;
template class nidas::core::SampleT<unsigned short>;
template class nidas::core::SampleT<int32_t>;
template class nidas::core::SampleT<uint32_t>;
template class nidas::core::SampleT<float>;
template class nidas::core::SampleT<double>;
template class nidas::core::SampleT<int64_t>;


Sample* nidas::core::getSample(sampleType type, unsigned int len)
{
    Sample* samp = 0;
    unsigned int lin = len;
    try {
        switch(type) {
        case CHAR_ST:
            samp = getSample<char>(len);
            break;
        case UCHAR_ST:
            samp = getSample<unsigned char>(len);
            break;
        case SHORT_ST:
            len /= sizeof(short);
            if (len * sizeof(short) != lin) return 0;
            samp = getSample<short>(len);
            break;
        case USHORT_ST:
            len /= sizeof(unsigned short);
            if (len * sizeof(unsigned short) != lin) return 0;
            samp = getSample<unsigned short>(len);
            break;
        case INT32_ST:
            len /= sizeof(int32_t);
            if (len * sizeof(int32_t) != lin) return 0;
            samp = getSample<int32_t>(len);
            break;
        case UINT32_ST:
            len /= sizeof(uint32_t);
            if (len * sizeof(uint32_t) != lin) return 0;
            samp = getSample<uint32_t>(len);
            break;
        case FLOAT_ST:
            len /= sizeof(float);
            if (len * sizeof(float) != lin) return 0;
            samp = getSample<float>(len);
            break;
        case DOUBLE_ST:
            len /= sizeof(double);
            if (len * sizeof(double) != lin) return 0;
            samp = getSample<double>(len);
            break;
        case INT64_ST:
            len /= sizeof(int64_t);
            if (len * sizeof(int64_t) != lin) return 0;
            samp = getSample<int64_t>(len);
            break;
        case UNKNOWN_ST:
        default:
            return 0;
        }
    }
    catch (const SampleLengthException& e) {
        return 0;
    }
    return samp;
}


SampleT<char>* nidas::core::getSample(const char* data)
{
    unsigned int len = strlen(data)+1;
    SampleT<char>* samp = 
        SamplePool<SampleT<char> >::getInstance()->getSample(len);
    strcpy(samp->getDataPtr(), data);
    return samp;
}


SampleChar::
SampleChar(const char* buffer)
{
    unsigned int len = strlen(buffer) + 1;
    allocateData(len);
    setDataLength(len);
    strcpy(getDataPtr(), buffer);
}
