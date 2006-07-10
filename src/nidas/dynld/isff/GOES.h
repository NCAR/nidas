/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-28 14:50:09 -0600 (Fri, 28 Oct 2005) $

    $LastChangedRevision: 3093 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Socket.h $
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
