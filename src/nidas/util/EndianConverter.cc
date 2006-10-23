
//
//              Copyright 2004 (C) by UCAR
//

#include <cassert>
#include <stdexcept>
#include <nidas/util/EndianConverter.h>

using namespace nidas::util;

/* static */
EndianConverter::endianness EndianConverter::hostEndianness =
	EndianConverter::privGetHostEndianness();

/* static */
EndianConverter* EndianConverter::flipConverter = new FlipConverter();

/* static */
EndianConverter* EndianConverter::noflipConverter = new NoFlipConverter();

/* static */
EndianConverter::endianness EndianConverter::privGetHostEndianness()
{
    endianness endian;
    union {
	long l;
	char c[4];
    } test;
    test.l = 1;
    if(test.c[3] == 1) endian = EC_BIG_ENDIAN;
    else if(test.c[0] == 1) endian = EC_LITTLE_ENDIAN;
    else throw std::runtime_error("unknown numeric endian-ness");

    // use compiler macros to double check.
#if __BYTE_ORDER == __BIG_ENDIAN
    assert(endian == EC_BIG_ENDIAN);
#endif
#if __BYTE_ORDER == __LITTLE_ENDIAN
    assert(endian == EC_LITTLE_ENDIAN);
#endif
    return endian;
}

/* static */
const EndianConverter* EndianConverter::getConverter(
	EndianConverter::endianness input,
	EndianConverter::endianness output)
{
    if (input == output) return noflipConverter;
    else return flipConverter;
}

/* static */
const EndianConverter* EndianConverter::getConverter(
	EndianConverter::endianness input)
{
    return getConverter(input,getHostEndianness());
}
