// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include <cassert>
#include <iostream>
#include <stdexcept>
#include "EndianConverter.h"
#include "Logger.h"

#include <cstring> // memcpy
#include <stdint.h>

using namespace nidas::util;
using namespace std;

namespace {

/**
 * Function for reading 8 bytes from an address,
 * flipping the bytes, and returning the double value.
 * Address does not need to be 8 byte aligned.
 */
inline double flipDoubleIn(const void* p)
{
    union {
      double v;
      char b[8];
    } u;
    const char* cp = (const char*)p;
    for (int i = 7; i >= 0; i--) u.b[i] = *cp++;
    return u.v;
}

inline double flipDouble(const double& p)
{
    return flipDoubleIn(&p);
}

/**
 * Function for reading 4 bytes from an address,
 * flipping the bytes, and returning the float value.
 * Address does not need to be 4 byte aligned.
 */
inline float flipFloatIn(const void* p)
{
    union {
      float v;
      char b[4];
    } u;
    const char* cp = (const char*)p;
    for (int i = 3; i >= 0; i--) u.b[i] = *cp++;
    return u.v;
}
inline float flipFloat(const float& p)
{
    return flipFloatIn(&p);
}


/**
 * Function for reading 8 bytes from an address,
 * flipping the bytes, and returning the int64_t value.
 * Address does not need to be 8 or 4 byte aligned.
 */
inline int64_t flipInt64In(const void* p)
{
    union {
      int64_t v;
      char b[8];
    } u;
    const char* cp = (const char*)p;
    for (int i = 7; i >= 0; i--) u.b[i] = *cp++;
    return u.v;
}
inline int64_t flipInt64(const int64_t& p)
{
    return flipInt64In(&p);
}


/**
 * Function for reading 4 bytes from an address,
 * flipping the bytes, and returning the 32 bit int value.
 * Address does not need to be 4 byte aligned.
 */
inline int32_t flipInt32In(const void* p)
{
    union {
      int32_t v;
      char b[4];
    } u;
    const char* cp = (const char*)p;
    for (int i = 3; i >= 0; i--) u.b[i] = *cp++;
    return u.v;
}
inline int32_t flipInt32(const int32_t& p)
{
    return flipInt32In(&p);
}


/**
 * Function for reading 4 bytes from an address,
 * flipping the bytes, and returning the unsigned 32 bit int value.
 * Address does not need to be 4 byte aligned.
 */
inline uint32_t flipUint32In(const void* p)
{
    union {
      uint32_t v;
      char b[4];
    } u;
    const char* cp = (const char*)p;
    for (int i = 3; i >= 0; i--) u.b[i] = *cp++;
    return u.v;
}
inline uint32_t flipUint32(const uint32_t& p)
{
    return flipUint32In(&p);
}


/**
 * Function for reading 2 bytes from an address,
 * flipping the bytes, and returning the 16 bit int value.
 * Address does not need to be 2 byte aligned.
 */
inline int16_t flipInt16In(const void* p)
{
    union {
      int16_t v;
      char b[2];
    } u;
    u.b[1] = ((const char*)p)[0];
    u.b[0] = ((const char*)p)[1];
    return u.v;
}
inline int16_t flipInt16(const int16_t& p)
{
    return flipInt16In(&p);
}


/**
 * Function for reading 2 bytes from an address,
 * flipping the bytes, and returning the unsigned 16 bit int value.
 * Address does not need to be 2 byte aligned.
 */
inline uint16_t flipUint16In(const void* p)
{
    union {
      uint16_t v;
      char b[2];
    } u;
    u.b[1] = ((const char*)p)[0];
    u.b[0] = ((const char*)p)[1];
    return u.v;
}
inline uint16_t flipUint16(const uint16_t& p)
{
    return flipUint16In(&p);
}


/**
 * Function for writing an 8 byte double value to
 * an address, flipping the bytes.
 * Address does not need to be 8 byte aligned.
 */
inline void flipDoubleOut(const double& v,void* p)
{
    union {
      double v;
      char b[8];
    } u;
    u.v = v;
    char* cp = (char*) p;
    for (int i = 7; i >= 0; i--) *cp++ = u.b[i];
}

/**
 * Function for writing a 4 byte float value to
 * an address, flipping the bytes.
 * Address does not need to be 4 byte aligned.
 */
inline void flipFloatOut(const float& v,void* p)
{
    union {
      float v;
      char b[4];
    } u;
    u.v = v;
    char* cp = (char*) p;
    for (int i = 3; i >= 0; i--) *cp++ = u.b[i];
}

/**
 * Function for writing an 8 byte, 64 bit int value to
 * an address, flipping the bytes.
 * Address does not need to be 8 or 4 byte aligned.
 */
inline void flipInt64Out(const int64_t& v,void* p)
{
    union {
      int64_t v;
      char b[8];
    } u;
    u.v = v;
    char* cp = (char*) p;
    for (int i = 7; i >= 0; i--) *cp++ = u.b[i];
}

/**
 * Function for writing a 4 byte, 32 bit int value to
 * an address, flipping the bytes.
 * Address does not need to be 4 byte aligned.
 */
inline void flipInt32Out(const int32_t& v,void* p)
{
    union {
      int32_t v;
      char b[4];
    } u;
    u.v = v;
    char* cp = (char*) p;
    for (int i = 3; i >= 0; i--) *cp++ = u.b[i];
}

/**
 * Function for writing a 4 byte unsigned 32 bit int value to
 * an address, flipping the bytes.
 */
inline void flipUint32Out(const uint32_t& v,void* p)
{
    union {
      uint32_t v;
      char b[4];
    } u;
    u.v = v;
    char* cp = (char*) p;
    for (int i = 3; i >= 0; i--) *cp++ = u.b[i];
}

/**
 * Function for writing a 2 byte 16 bit int value to
 * an address, flipping the bytes.
 */
inline void flipInt16Out(const int16_t &v,void* p)
{
    union {
      int16_t v;
      char b[2];
    } u;
    u.v = v;
    char* cp = (char*) p;
    cp[0] = u.b[1];
    cp[1] = u.b[0];
}

/**
 * Function for writing a 2 byte unsigned 16 bit int value to
 * an address, flipping the bytes.
 */
inline void flipUint16Out(const uint16_t& v,void *p)
{
    union {
      uint16_t v;
      char b[2];
    } u;
    u.v = v;
    char* cp = (char*) p;
    cp[0] = u.b[1];
    cp[1] = u.b[0];
}

} // namespace


namespace nidas { namespace util {

/**
 * EndianConverter that flips bytes, used for 
 * conversion of little-to-big and big-to-little.
 */
class FlipConverter : public EndianConverter {
public:

    virtual ~FlipConverter() {}

    double doubleValue(const void* p) const
    {
        return flipDoubleIn(p);
    }

    double doubleValue(const double& p) const
    {
        return flipDouble(p);
    }

    float floatValue(const void* p) const
    {
        return flipFloatIn(p);
    }

    float floatValue(const float& p) const
    {
        return flipFloat(p);
    }

    int64_t int64Value(const void* p) const
    {
        return flipInt64In(p);
    }

    int64_t int64Value(const int64_t& p) const
    {
        return flipInt64(p);
    }

    int32_t int32Value(const void* p) const
    {
        return flipInt32In(p);
    }

    int32_t int32Value(const int32_t& p) const
    {
        return flipInt32(p);
    }

    uint32_t uint32Value(const void* p) const
    {
        return flipUint32In(p);
    }

    uint32_t uint32Value(const uint32_t& p) const
    {
        return flipUint32(p);
    }

    int16_t int16Value(const void* p) const
    {
        return flipInt16In(p);
    }

    int16_t int16Value(const int16_t& p) const
    {
        return flipInt16(p);
    }

    uint16_t uint16Value(const void* p) const
    {
        return flipUint16In(p);
    }

    uint16_t uint16Value(const uint16_t& p) const
    {
        return flipUint16(p);
    }

    void doubleCopy(const double& v,void* p) const
    {
        return flipDoubleOut(v,p);
    }

    void floatCopy(const float& v,void* p) const
    {
        return flipFloatOut(v,p);
    }

    void int64Copy(const int64_t& v,void* p) const
    {
        return flipInt64Out(v,p);
    }

    void int32Copy(const int32_t& v,void* p) const
    {
        return flipInt32Out(v,p);
    }

    void uint32Copy(const uint32_t& v,void* p) const
    {
        return flipUint32Out(v,p);
    }

    void int16Copy(const int16_t &v,void* p) const
    {
        return flipInt16Out(v,p);
    }

    void uint16Copy(const uint16_t& v,void *p) const
    {
        return flipUint16Out(v,p);
    }
};

/**
 * EndianConverter that doesn't flip bytes.
 */
class NoFlipConverter : public EndianConverter {
public:
    virtual ~NoFlipConverter() {}

    double doubleValue(const void* p) const
    {
        double v;
        memcpy(&v,p,sizeof(double));
        return v;
    }

    double doubleValue(const double& p) const
    {
        return p;
    }

    float floatValue(const void* p) const
    {
        float v;
        memcpy(&v,p,sizeof(float));
        return v;
    }

    float floatValue(const float& p) const
    {
        return p;
    }

    int64_t int64Value(const void* p) const
    {
        int64_t v;
        memcpy(&v,p,sizeof(int64_t));
        return v;
    }

    int64_t int64Value(const int64_t& p) const
    {
        return p;
    }

    int32_t int32Value(const void* p) const
    {
        int32_t v;
        memcpy(&v,p,sizeof(int32_t));
        return v;
    }

    int32_t int32Value(const int32_t& p) const
    {
        return p;
    }

    uint32_t uint32Value(const void* p) const
    {
        uint32_t v;
        memcpy(&v,p,sizeof(uint32_t));
        return v;
    }

    uint32_t uint32Value(const uint32_t& p) const
    {
        return p;
    }

    int16_t int16Value(const void* p) const
    {
        union {
          int16_t v;
          char b[2];
        } u;
        u.b[0] = ((const char*)p)[0];
        u.b[1] = ((const char*)p)[1];
        return u.v;
    }

    int16_t int16Value(const int16_t& p) const
    {
        return p;
    }

    uint16_t uint16Value(const void* p) const
    {
        union {
          uint16_t v;
          char b[2];
        } u;
        u.b[0] = ((const char*)p)[0];
        u.b[1] = ((const char*)p)[1];
        return u.v;
    }

    uint16_t uint16Value(const uint16_t& p) const
    {
        return p;
    }

    void doubleCopy(const double& v,void* p) const
    {
        memcpy(p,&v,sizeof(double));
    }

    void floatCopy(const float& v,void* p) const
    {
        memcpy(p,&v,sizeof(float));
    }

    void int64Copy(const int64_t& v,void* p) const
    {
        memcpy(p,&v,sizeof(int64_t));
    }

    void int32Copy(const int& v,void* p) const
    {
        memcpy(p,&v,sizeof(int32_t));
    }

    void uint32Copy(const uint32_t& v,void* p) const
    {
        memcpy(p,&v,sizeof(uint32_t));
    }

    void int16Copy(const int16_t &v,void* p) const
    {
        memcpy(p,&v,sizeof(int16_t));
#ifdef BOZO
        union {
          int16_t v;
          char b[2];
        } u;
        u.v = v;
        ((char*)p)[0] = u.b[1];
        ((char*)p)[1] = u.b[0];
#endif
    }

    void uint16Copy(const uint16_t& v,void* p) const {
        memcpy(p,&v,sizeof(uint16_t));
#ifdef BOZO
        union {
          uint16_t v;
          char b[2];
        } u;
        u.v = v;
        ((char*)p)[0] = u.b[1];
        ((char*)p)[1] = u.b[0];
#endif
    }
};

}} // namespace nidas::util


namespace {

EndianConverter::endianness privGetHostEndianness()
{
    EndianConverter::endianness endian;
    union {
        long l;
        char c[4];
    } test;
    test.l = 1;
    if(test.c[3] == 1) endian = EndianConverter::EC_BIG_ENDIAN;
    else if(test.c[0] == 1) endian = EndianConverter::EC_LITTLE_ENDIAN;
    else throw std::runtime_error("unknown numeric endian-ness");

    // use compiler macros to double check.
    if (__BYTE_ORDER == __BIG_ENDIAN)
        assert(endian == EndianConverter::EC_BIG_ENDIAN);
    else if (__BYTE_ORDER == __LITTLE_ENDIAN)
        assert(endian == EndianConverter::EC_LITTLE_ENDIAN);

    VLOG(("endian=")
        << (endian == EndianConverter::EC_LITTLE_ENDIAN ? "little" :
            (endian == EndianConverter::EC_BIG_ENDIAN ? "big" : "unknown")));
    return endian;
}

} // anon namespace


/* static */
EndianConverter::endianness EndianConverter::getHostEndianness()
{
    static EndianConverter::endianness hostEndianness =
        privGetHostEndianness();
    return hostEndianness;
}

/* static */
const EndianConverter* EndianConverter::getConverter(
    EndianConverter::endianness input,
    EndianConverter::endianness output)
{
    static NoFlipConverter noflipConverter;
    static FlipConverter flipConverter;

    if (input == output)
        return &noflipConverter;
    else
        return &flipConverter;
}

/* static */
const EndianConverter* EndianConverter::getConverter(
    EndianConverter::endianness input)
{
    return getConverter(input, getHostEndianness());
}

EndianConverter::~EndianConverter()
{}
