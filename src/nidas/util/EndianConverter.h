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

#include <cstring> // memcpy

#include <nidas/util/ThreadSupport.h>

namespace nidas { namespace util {

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
    static endianness getHostEndianness();

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
}}	// namespace nidas namespace util
#endif

