
//
//              Copyright 2004 (C) by UCAR
//

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <nidas/util/EndianConverter.h>

using namespace nidas::util;

/* static */
Mutex EndianConverter::staticInitMutex = Mutex();

/* static */
EndianConverter::endianness EndianConverter::hostEndianness = EndianConverter::EC_UNKNOWN_ENDIAN;

/* static */
EndianConverter* EndianConverter::flipConverter = 0;

/* static */
EndianConverter* EndianConverter::noflipConverter = 0;

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
    // since this method is itself static, we don't know if the
    // static initializers for noFlipConverter, flipConverter, and
    // hostEndianness have been completed, since this could
    // itself have been called from a static initializer.
    // I assume the order of execution of static initializers is not defined.
    //
    // This appeared to be the case when this getConverter() method was
    // called from a static initializer, because this method was
    // returning a null pointer for noflipConverter, even though
    // it was being initialized with noflipConverter= new NoFlipConverter() above.
    //
    // We are relying on the initialization of staticInitMutex here though...
    
    staticInitMutex.lock();
    if (!noflipConverter) noflipConverter = new NoFlipConverter();
    if (!flipConverter) flipConverter = new FlipConverter();
    hostEndianness = privGetHostEndianness();
    staticInitMutex.unlock();
    
    if (input == output) return noflipConverter;
    else return flipConverter;
}

/* static */
const EndianConverter* EndianConverter::getConverter(
	EndianConverter::endianness input)
{
    return getConverter(input,getHostEndianness());
}
