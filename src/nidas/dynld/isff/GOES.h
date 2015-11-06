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

#ifndef NIDAS_DYNLD_ISFF_GOES_H
#define NIDAS_DYNLD_ISFF_GOES_H


namespace nidas { namespace dynld { namespace isff {

// using namespace nidas::core;

/**
 * Support for a GOES transmitter, implemented as an IOChannel.
 */

class GOES {

public:

    /**
     * Encode a single precision float for transfer over GOES.
     * The encoded value occupies 5 bytes. The 2 most
     * significant bits each encoded byte are of 01 (hex 0x40)
     * in order to fit in the restricted GOES tranmission character set.
     * The other 6 bits in each byte used to store the floating
     * point value.  The least significant 2 bits of the mantissa
     * are discarded (resulting in a 21 bit mantissa), in order to
     * code the number into 30 bits total: * 5 bytes X 6 data bits/byte.
     * The resultant precision is 1 part in 2 million (2^21),
     * reduced from 1 part in 8 million (2^23).
     * The nan value is encoded and decoded correctly.
     */
    static void float_encode_5x6(float f,char* enc);

    /**
     * Decode 5x6 bit value back into a floating point.
     */
    static float float_decode_5x6(const char* enc);

    /**
     * Encode a 32 bit single precision float for transfer over GOES.
     * The encoded value occupies 4 bytes. The 2 most
     * significant bits each encoded byte are of 01 (hex 0x40)
     * in order to fit in the restricted GOES tranmission character set.
     * The other 6 bits in each byte used to store the floating
     * point value.  The least significant 8 bits of the mantissa
     * are discarded (resulting in a 15 bit mantissa), in order to
     * code the number into 24 bits total: 4 bytes X 6 data bits/byte.
     * The resulting precision is 1 part in 32768  (2^15).
     * The nan value is encoded and decoded correctly.
     */
    static void float_encode_4x6(float f,char* enc);

    /**
     * Decode 4x6 bit value back into a floating point.
     */
    static float float_decode_4x6(const char* enc);


};


}}}	// namespace nidas namespace dynld namespace isff

#endif
