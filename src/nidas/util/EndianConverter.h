/*              Copyright (C) 1989,90,91,92,93,94 by UCAR
 *
 * File       : $RCSfile: UTime.h,v $
 * Revision   : $Revision$
 * Directory  : $Source: /code/cvs/isa/src/lib/atdUtil/UTime.h,v $
 * System     : ASTER
 * Author     : Gordon Maclean
 * Date       : $Date$
 *
 * Description:
 */

#ifndef NIDAS_UTIL_ENDIANCONVERTER_H
#define NIDAS_UTIL_ENDIANCONVERTER_H

#include <string.h> // memcpy

#include <nidas/util/ThreadSupport.h>

namespace nidas { namespace util {

/**
 * Function for reading 8 bytes from an address,
 * flipping the bytes, and returning the double value.
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

/**
 * Function for reading 4 bytes from an address,
 * flipping the bytes, and returning the float value.
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

/**
 * Function for reading 8 bytes from an address,
 * flipping the bytes, and returning the long long value.
 */
inline long long flipLonglongIn(const void* p)
{
    union {
      long long v;
      char b[8];
    } u;
    const char* cp = (const char*)p;
    for (int i = 7; i >= 0; i--) u.b[i] = *cp++;
    return u.v;
}

/**
 * Function for reading 4 bytes from an address,
 * flipping the bytes, and returning the long int value.
 */
inline long flipLongIn(const void* p)
{
    union {
      long v;
      char b[4];
    } u;
    const char* cp = (const char*)p;
    for (int i = 3; i >= 0; i--) u.b[i] = *cp++;
    return u.v;
}

/**
 * Function for reading 4 bytes from an address,
 * flipping the bytes, and returning the unsigned long int value.
 */
inline unsigned long flipUlongIn(const void* p)
{
    union {
      unsigned long v;
      char b[4];
    } u;
    const char* cp = (const char*)p;
    for (int i = 3; i >= 0; i--) u.b[i] = *cp++;
    return u.v;
}

/**
 * Function for reading 2 bytes from an address,
 * flipping the bytes, and returning the short int value.
 */
inline short flipShortIn(const void* p)
{
    union {
      short v;
      char b[2];
    } u;
    u.b[1] = ((const char*)p)[0];
    u.b[0] = ((const char*)p)[1];
    return u.v;
}

/**
 * Function for reading 2 bytes from an address,
 * flipping the bytes, and returning the unsigned short int value.
 */
inline unsigned short flipUshortIn(const void* p)
{
    union {
      unsigned short v;
      char b[2];
    } u;
    u.b[1] = ((const char*)p)[0];
    u.b[0] = ((const char*)p)[1];
    return u.v;
}

/**
 * Function for writing an 8 byte double value to
 * an address, flipping the bytes.
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
 * Function for writing an 8 byte long long value to
 * an address, flipping the bytes.
 */
inline void flipLonglongOut(const long long& v,void* p)
{
    union {
      long long v;
      char b[8];
    } u;
    u.v = v;
    char* cp = (char*) p;
    for (int i = 7; i >= 0; i--) *cp++ = u.b[i];
}

/**
 * Function for writing a 4 byte long value to
 * an address, flipping the bytes.
 */
inline void flipLongOut(const long& v,void* p)
{
    union {
      long v;
      char b[4];
    } u;
    u.v = v;
    char* cp = (char*) p;
    for (int i = 3; i >= 0; i--) *cp++ = u.b[i];
}

/**
 * Function for writing a 4 byte unsigned long value to
 * an address, flipping the bytes.
 */
inline void flipUlongOut(const unsigned long& v,void* p)
{
    union {
      unsigned long v;
      char b[4];
    } u;
    u.v = v;
    char* cp = (char*) p;
    for (int i = 3; i >= 0; i--) *cp++ = u.b[i];
}

/**
 * Function for writing a 2 byte short int value to
 * an address, flipping the bytes.
 */
inline void flipShortOut(const short &v,void* p)
{
    union {
      short v;
      char b[2];
    } u;
    u.v = v;
    char* cp = (char*) p;
    cp[0] = u.b[1];
    cp[1] = u.b[0];
}

/**
 * Function for writing a 2 byte unsigned short int value to
 * an address, flipping the bytes.
 */
inline void flipUshortOut(const unsigned short& v,void *p)
{
    union {
      unsigned short v;
      char b[2];
    } u;
    u.v = v;
    char* cp = (char*) p;
    cp[0] = u.b[1];
    cp[1] = u.b[0];
}

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

    virtual ~EndianConverter() {}

    enum endianness { EC_UNKNOWN_ENDIAN, EC_BIG_ENDIAN, EC_LITTLE_ENDIAN };

    // typedef enum endianness endianness_t;

    static endianness hostEndianness;

    /**
     * Return endianness value for this host.
     */
    static endianness getHostEndianness() { return hostEndianness; }

    /**
     * Return an EndianConverter for converting from one endian to
     * another.  If both are the same, then the converter that is
     * returned just does memcpy's and does not change the representation.
     */
    static const EndianConverter* getConverter(endianness input, endianness output);

    /**
     * Return an EndianConverter for converting from an endian
     * represenation to the endian representation of th host.
     * If both are the same, then the converter that is
     * returned just does memcpy's and does not change the representation.
     */
    static const EndianConverter* getConverter(endianness input);

    /**
     * Get 8 byte double at address, do endian conversion.
     * Pointer to address does not need to be 8-byte aligned.
     */
    virtual double doubleValue(const void* ) const = 0;

    /**
     * Get 4 byte float at address, do endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual float floatValue(const void* ) const = 0;

    /**
     * Get 4 byte long at address, do endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual long longValue(const void* ) const = 0;

    /**
     * Get 8 byte long long at address, do endian conversion.
     * Pointer to address does not need to be 8-byte aligned.
     */
    virtual long long longlongValue(const void* ) const = 0;

    /**
     * Get 4 byte unsigned long at address, do endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual unsigned long ulongValue(const void* ) const = 0;

    virtual short shortValue(const void* ) const = 0;

    virtual unsigned short ushortValue(const void* ) const = 0;

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
     * Copy 4 byte long to the given address, doing endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual void longCopy(const long&,void* ) const = 0;

    /**
     * Copy 8 byte long long to the given address, doing endian conversion.
     * Pointer to address does not need to be 8-byte aligned.
     */
    virtual void longlongCopy(const long long&,void* ) const = 0;

    /**
     * Copy 4 byte unsigned long to the given address, doing endian conversion.
     * Pointer to address does not need to be 4-byte aligned.
     */
    virtual void ulongCopy(const unsigned long&, void* ) const = 0;

    virtual void shortCopy(const short&,void* ) const = 0;

    virtual void ushortCopy(const unsigned short&, void* ) const = 0;

private:
    static endianness privGetHostEndianness();

    static EndianConverter* flipConverter;

    static EndianConverter* noflipConverter;

    static Mutex staticInitMutex;
};

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

    float floatValue(const void* p) const
    {
        return flipFloatIn(p);
    }

    long long longlongValue(const void* p) const
    {
        return flipLonglongIn(p);
    }

    long longValue(const void* p) const
    {
        return flipLongIn(p);
    }

    unsigned long ulongValue(const void* p) const
    {
        return flipUlongIn(p);
    }

    short shortValue(const void* p) const
    {
        return flipShortIn(p);
    }

    unsigned short ushortValue(const void* p) const
    {
        return flipUshortIn(p);
    }

    void doubleCopy(const double& v,void* p) const
    {
        return flipDoubleOut(v,p);
    }

    void floatCopy(const float& v,void* p) const
    {
        return flipFloatOut(v,p);
    }

    void longlongCopy(const long long& v,void* p) const
    {
        return flipLonglongOut(v,p);
    }

    void longCopy(const long& v,void* p) const
    {
        return flipLongOut(v,p);
    }

    void ulongCopy(const unsigned long& v,void* p) const
    {
        return flipUlongOut(v,p);
    }

    void shortCopy(const short &v,void* p) const
    {
        return flipShortOut(v,p);
    }

    void ushortCopy(const unsigned short& v,void *p) const
    {
        return flipUshortOut(v,p);
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

    float floatValue(const void* p) const
    {
        float v;
        memcpy(&v,p,sizeof(float));
        return v;
    }

    long long longlongValue(const void* p) const
    {
        long long v;
        memcpy(&v,p,sizeof(long long));
        return v;
    }

    long longValue(const void* p) const
    {
        long v;
        memcpy(&v,p,sizeof(long));
        return v;
    }

    unsigned long ulongValue(const void* p) const
    {
        unsigned long v;
        memcpy(&v,p,sizeof(long));
        return v;
    }

    short shortValue(const void* p) const
    {
        union {
          short v;
          char b[2];
        } u;
        u.b[0] = ((const char*)p)[0];
        u.b[1] = ((const char*)p)[1];
        return u.v;
    }

    unsigned short ushortValue(const void* p) const
    {
        union {
          unsigned short v;
          char b[2];
        } u;
        u.b[0] = ((const char*)p)[0];
        u.b[1] = ((const char*)p)[1];
        return u.v;
    }

    void doubleCopy(const double& v,void* p) const
    {
        memcpy(p,&v,sizeof(double));
    }

    void floatCopy(const float& v,void* p) const
    {
        memcpy(p,&v,sizeof(float));
    }

    void longlongCopy(const long long& v,void* p) const
    {
        memcpy(p,&v,sizeof(long long));
    }

    void longCopy(const long& v,void* p) const
    {
        memcpy(p,&v,sizeof(long));
    }

    void ulongCopy(const unsigned long& v,void* p) const
    {
        memcpy(p,&v,sizeof(unsigned long));
    }

    void shortCopy(const short &v,void* p) const
    {
        union {
          short v;
          char b[2];
        } u;
        u.v = v;
        ((char*)p)[0] = u.b[1];
        ((char*)p)[1] = u.b[0];
    }

    void ushortCopy(const unsigned short& v,void* p) const {
        union {
          unsigned short v;
          char b[2];
        } u;
        u.v = v;
        ((char*)p)[0] = u.b[1];
        ((char*)p)[1] = u.b[0];
    }
};
}}	// namespace nidas namespace util
#endif

