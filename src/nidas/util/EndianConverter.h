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

#ifndef NIDAS_UTIL_ENDIANCONVERTER_H
#define NIDAS_UTIL_ENDIANCONVERTER_H

#include <cstdint> // uint32_t

namespace nidas { namespace util {


/**
 * Virtual base class declaring methods for converting
 * numeric values between little-endian and big-endian representations,
 * and for determining the endian represenation of the host system.
 * Implementations of this class are meant for serializing/de-serializing
 * binary data. The methods read from an address or write to an address.
 * The addresses do not have to be aligned correctly for the
 * data value type.
 */
class EndianConverter {
public:

    virtual ~EndianConverter();

    enum endianness { EC_UNKNOWN_ENDIAN, EC_BIG_ENDIAN, EC_LITTLE_ENDIAN };

    /**
     * Return endianness value for this host.
     */
    static endianness getHostEndianness();

    /**
     * Return an EndianConverter for converting from one endian to
     * another.  If both are the same, then the converter that is
     * returned just does memcpy's and does not change the representation.
     * It is a const pointer since all its methods are const, and the
     * pointer is owned by EndianConverter.
     */
    static const EndianConverter* getConverter(endianness input, endianness output);

    /**
     * Return an EndianConverter for converting from an endian
     * represenation to the endian representation of this host.
     * If both are the same, then the converter that is
     * returned just does memcpy's and does not change the representation.
     * It is a const pointer since all its methods are const, and the
     * pointer is owned by EndianConverter.
     */
    static const EndianConverter* getConverter(endianness input);

    /**
     * Get 8 byte double at address, do endian conversion.
     * Pointer to address does not need to be 8-byte aligned.
     */
    virtual double doubleValue(const void* ) const = 0;

    virtual double doubleValue(const double& ) const = 0;

    /**
     * Get 4 byte float at address, do endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual float floatValue(const void* ) const = 0;

    virtual float floatValue(const float& ) const = 0;

    /**
     * Get 4 byte int32 at address, do endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual int32_t int32Value(const void* ) const = 0;

    virtual int32_t int32Value(const int32_t& ) const = 0;

    /**
     * Get 8 byte int64_t at address, do endian conversion.
     * Pointer to address does not need to be 8-byte aligned.
     */
    virtual int64_t int64Value(const void* ) const = 0;

    virtual int64_t int64Value(const int64_t& ) const = 0;

    /**
     * Get 4 byte unsigned int32_t at address, do endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual uint32_t uint32Value(const void* ) const = 0;

    virtual uint32_t uint32Value(const uint32_t& ) const = 0;

    virtual int16_t int16Value(const void* ) const = 0;

    virtual int16_t int16Value(const int16_t& ) const = 0;

    virtual uint16_t uint16Value(const void* ) const = 0;

    virtual uint16_t uint16Value(const uint16_t& ) const = 0;

    /**
     * Copy 8 byte double to the given address, doing endian conversion.
     * Pointer to address does not need to be 8-byte aligned.
     */
    virtual void doubleCopy(const double&,void* ) const = 0;

    /**
     * Copy 4 byte float to the given address, doing endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual void floatCopy(const float&,void* ) const = 0;

    /**
     * Copy 4 byte int to the given address, doing endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual void int32Copy(const int32_t&,void* ) const = 0;

    /**
     * Copy 8 byte int64_t to the given address, doing endian conversion.
     * Pointer to address does not need to be 8-byte aligned.
     */
    virtual void int64Copy(const int64_t&,void* ) const = 0;

    /**
     * Copy 4 byte unsigned int to the given address, doing endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual void uint32Copy(const uint32_t&, void* ) const = 0;

    virtual void int16Copy(const int16_t&,void* ) const = 0;

    virtual void uint16Copy(const uint16_t&, void* ) const = 0;

};

}}	// namespace nidas namespace util
#endif

