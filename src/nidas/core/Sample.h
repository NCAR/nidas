// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    Some over-engineered sample classes.
 ********************************************************************

*/

#ifndef NIDAS_CORE_SAMPLET_H
#define NIDAS_CORE_SAMPLET_H

#include <nidas/core/SampleLengthException.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/time_constants.h>
#include <nidas/linux/types.h>

#include <climits>
#include <iostream>
#include <cstring>

#include <cmath>

namespace nidas { namespace core {

/**
 * Value of a float NAN for general use.
 */
extern const float floatNAN;

extern const float doubleNAN;

/**
 * Posix time in microseconds, the number of non-leap microseconds since 1970 Jan 1 00:00 UTC
 */
typedef long long dsm_time_t;

typedef unsigned int dsm_sample_id_t;

/**
 * macros to get and set fields of the tid member of a Sample.
 * The 32 bit unsigned int tid is made of two fields:
 *	26 least significant bits containing the FULL_ID
 *	6 most significant bits containing a SAMPLE_TYPE enumeration (0-63)
 * The FULL_ID field is further split into a DSM id and
 * sensor-plus-sample (SPS) id:
 *	10 most significant bits containing the DSM_ID (0-1023)
 *	16 least significant bits containing the SPS_ID (0-65535)
 */
#define GET_SAMPLE_TYPE(tid) ((tid) >> 26)
#define SET_SAMPLE_TYPE(tid,val) (((tid) & 0x03ffffff) | ((unsigned int)(val) << 26))

#define GET_FULL_ID(tid) ((tid) & 0x03ffffff)
#define SET_FULL_ID(tid,val) (((tid) & 0xfc000000) | ((val) & 0x03ffffff))

#define GET_DSM_ID(tid) (((tid) & 0x03ff0000) >> 16)
#define SET_DSM_ID(tid,val) (((tid) & 0xfc00ffff) | (((unsigned int)(val) & 0x3ff) << 16))

#define GET_SPS_ID(tid) ((tid) & 0xffff)
#define GET_SHORT_ID(tid) ((tid) & 0xffff)

#define SET_SPS_ID(tid,val) (((tid) & 0xffff0000) | ((val) & 0xffff)) 
#define SET_SHORT_ID(tid,val) (((tid) & 0xffff0000) | ((val) & 0xffff)) 

/**
 * Whether to use mutexes to make sure the reference count
 * increments and decrement-and-test operations are atomic.
 */
#define MUTEX_PROTECT_REF_COUNTS

/**
 * The gcc buildin atomic operations are not supported on arm, and
 * one must use -march=i686 for them to work on 32 bit x86.
 */
// #define USE_ATOMIC_REF_COUNT

/**
 * maxValue is an overloaded function returning the
 * maximum value of its integer argument.
 */
inline unsigned int maxValue(unsigned short)
{
    return USHRT_MAX;
}

inline unsigned int maxValue(short)
{
    return SHRT_MAX;
}

inline unsigned int maxValue(int)
{
    return INT_MAX;
}

inline unsigned int maxValue(unsigned int)
{
    return UINT_MAX;
}

typedef enum sampleType {
	CHAR_ST, UCHAR_ST, SHORT_ST, USHORT_ST,
	INT32_ST, UINT32_ST, FLOAT_ST, DOUBLE_ST,
	INT64_ST, UNKNOWN_ST } sampleType;

/**
 * Overloaded function to return a enumerated value
 * corresponding to the type pointed to by the argument.
 */
inline sampleType getSampleType(char*)
{
    return CHAR_ST;
}

inline sampleType getSampleType(unsigned char*)
{
    return UCHAR_ST;
}

inline sampleType getSampleType(unsigned short*)
{
    return USHORT_ST;
}

inline sampleType getSampleType(short*)
{
    return SHORT_ST;
}

inline sampleType getSampleType(unsigned int*)
{
    return UINT32_ST;
}

inline sampleType getSampleType(int*)
{
    return INT32_ST;
}

inline sampleType getSampleType(float*)
{
    return FLOAT_ST;
}

inline sampleType getSampleType(double*)
{
    return DOUBLE_ST;
}

inline sampleType getSampleType(long long*)
{
    return INT64_ST;
}

inline sampleType getSampleType(void*)
{
    return UNKNOWN_ST;
}

/**
 * The header fields of a Sample: a time_tag, a data length field,
 * and an identifier.
 */
class SampleHeader {
public:

    SampleHeader(sampleType t=CHAR_ST) :
    	_tt(0),_length(0),_tid((unsigned int)t << 26) {}

    dsm_time_t getTimeTag() const { return _tt; }

    void setTimeTag(dsm_time_t val) { _tt = val; }

    /**
     * Get the value of the length member of the header. This
     * is the length in bytes of the data portion of the sample.
     */
    unsigned int getDataByteLength() const { return _length; }

    /**
     * Set the length member of the header. This is the length
     * in bytes.
     */
    void setDataByteLength(unsigned int val) { _length = val; }

    dsm_sample_id_t getId() const { return GET_FULL_ID(_tid); }
    void setId(dsm_sample_id_t val) { _tid = SET_FULL_ID(_tid,val); }

    dsm_sample_id_t getRawId() const { return _tid; }
    void setRawId(dsm_sample_id_t val) { _tid = val; }

    /**
     * Get the DSM identifier for the sample.
     */
    unsigned int getDSMId() const { return GET_DSM_ID(_tid); }
    void setDSMId(unsigned int val) { _tid = SET_DSM_ID(_tid,val); }

    /**
     * Get the sample identifier for the sample.
     */
    unsigned int getSpSId() const { return GET_SPS_ID(_tid); }
    void setSpSId(unsigned int val) { _tid = SET_SPS_ID(_tid,val); }

    /**
     * Get the data type of this sample. The type can only be set in the 
     * constructor.
     */
    unsigned char getType() const { return GET_SAMPLE_TYPE(_tid); }

    static unsigned int getSizeOf()
    {
        return sizeof(dsm_time_t) + sizeof(dsm_sample_length_t) +
		sizeof(dsm_sample_id_t); }

    static unsigned int getMaxDataLength() { return maxValue(dsm_sample_length_t()); }

protected:

    /**
     * Time-tag in non-leap microseconds since Jan 1, 1970 00:00 GMT.
     */
    dsm_time_t _tt; 

    /**
     * Length of data (# of bytes) in the sample - does not include
     * header fields
     */
    dsm_sample_length_t _length;

    /**
     * An identifier for this sample consisting of packed bit fields.
     * The most significant 6 bits are a data type enumeration
     * (float, double etc), which is accessed with set/getType().
     *
     * The other 26 bits are the sample identifier, which is further
     * broken into 10 bits of a DSM identifier, acccessed with get/setDSMId(),
     * and 16 bits of a sensor/sample identifier, accessed with
     * get/setSpSId().
     */
    dsm_sample_id_t _tid;
};

/**
 * Interface to a data sample.  A Sample contains
 * a SampleHeader and some data.
 */
class Sample {
public:
  
    Sample(sampleType t = CHAR_ST) :
        _header(t),_refCount(1),_refLock()
    {
        _nsamps++;
    }

    virtual ~Sample() { _nsamps--; }

    void setTimeTag(dsm_time_t val) { _header.setTimeTag(val); }

    /**
     * Time-tag in non-leap microseconds since Jan 1, 1970 00:00 GMT.
     */
    dsm_time_t getTimeTag() const { return _header.getTimeTag(); }

    /**
     * Set the id portion of the sample header. The id 
     * typically identifies the data system and
     * sensor of origin of the sample.
     */
    void setId(dsm_sample_id_t val) { _header.setId(val); }

    /**
     * Get the id portion of the sample header.
     */
    dsm_sample_id_t getId() const { return _header.getId(); }

    /**
     * Set the full, raw id portion of the sample header.
     * This method is not typically used except when
     * doing raw IO on a sample. The raw id
     * contains an enumeration of the sample type,
     * along with the fields returned by getId().
     */
    void setRawId(dsm_sample_id_t val) { _header.setRawId(val); }

    dsm_sample_id_t getRawId() const { return _header.getRawId(); }

    /**
     * Set the short id portion of the sample header, containing
     * the sensor + sample ids.
     * This is the portion of the id without the DSM id.
     */
    void setSpSId(unsigned int val) { _header.setSpSId(val); }

    /**
     * Get the short id portion of the sample header.
     * This is the portion of the id without the DSM id.
     */
    unsigned int getSpSId() const { return _header.getSpSId(); }

    /**
     * Set the DSM (data system) id portion of the sample header.
     */
    void setDSMId(unsigned int val) { _header.setDSMId(val); }

    /**
     * Get the DSM (data system) id portion of the sample header.
     */
    unsigned int getDSMId() const { return _header.getDSMId(); }

    /**
     * Get the number of bytes in data portion of sample.
     */
    unsigned int getDataByteLength() const { return _header.getDataByteLength(); }

    /**
     * Set the number of elements in data portion of sample.
     */
    virtual void setDataLength(unsigned int val)
    	throw(SampleLengthException) = 0;
 
    /**
     * Get the number of elements in data portion of sample.
     */
    virtual unsigned int getDataLength() const = 0;

    /**
     * Get the type of the sample.
     */
    virtual sampleType getType() const = 0;

    /**
     * Number of bytes in header.
     */
    unsigned int getHeaderLength() const { return SampleHeader::getSizeOf(); }

    /**
     * Get a pointer to the header portion of the sample.
     */
    const void* getHeaderPtr() const { return &_header; }

    /**
     * Get a void* pointer to the data portion of the sample.
     */
    virtual void* getVoidDataPtr() = 0;

    /**
     * Get a const void* pointer to the data portion of the sample.
     */
    virtual const void* getConstVoidDataPtr() const = 0;

    /**
     * Get the numeric value of data element i.
     * No range checking of i is done.
     */
    virtual double getDataValue(unsigned int i) const = 0;

    /**
     * Set the value of data element i to a double.
     * No range checking of i is done.
     */
    virtual void setDataValue(unsigned int i,double val) = 0;

    /**
     * Set the value of data element i to a float.
     * No range checking is of i done.
     */
    virtual void setDataValue(unsigned int i,float val) = 0;

    /**
     * Get number of elements allocated in data portion of sample.
     */
    virtual unsigned int getAllocLength() const = 0;

    /**
     * Get number of bytes allocated in data portion of sample.
     */
    virtual unsigned int getAllocByteLength() const = 0;

    /**
     * Allocate a number of bytes of data.
     */
    virtual void allocateData(unsigned int val) throw(SampleLengthException) = 0;

    /**
     * Re-allocate a number of bytes of data, saving old contents.
     */
    virtual void reallocateData(unsigned int val) throw(SampleLengthException) = 0;

    /**
     * Increment the reference count for this sample.
     * Sample supports a form of reference counting.
     * See freeReference.  We have to use a hack-like
     * cast of the this pointer to a const so that holdReference
     * can be used on a const Sample.  The SamplePool class
     * supports Sample reference counting.
     */
    void holdReference() const {
#ifdef USE_ATOMIC_REF_COUNT
        __sync_add_and_fetch(&_refCount,1);
#else
#ifdef MUTEX_PROTECT_REF_COUNTS
	_refLock.lock();
#endif
        _refCount++;
#ifdef MUTEX_PROTECT_REF_COUNTS
	_refLock.unlock();
#endif
#endif
    }

    /**
     * Decrement the reference count for this sample.
     */
    virtual void freeReference() const = 0;

protected:

    SampleHeader _header;

    /**
     * The reference count.
     */
    mutable int _refCount;

#ifdef MUTEX_PROTECT_REF_COUNTS
    mutable nidas::util::Mutex _refLock;
#endif

    /**
     * Global count of the number of samples in use by a process.
     * Incremented in the constructor, decremented in the destructor.
     * Useful for development debugging to track leaks.
     */
    static int _nsamps;
};

/**
 * A typed Sample, with data of type DataT.
 */
template <class DataT>
class SampleT : public Sample {
public:

    SampleT() : Sample(getType()),_data(0),_allocLen(0) {}

    ~SampleT() { delete [] _data; }

    sampleType getType() const { return getSampleType(_data); }

    /**
     * Get number of elements of type DataT in data.
     */
    unsigned int getDataLength() const
    {
        return getDataByteLength() / sizeof(DataT);
    }

    /**
     * Set the number of elements of type DataT in data.
     * @param val: number of elements.
     */
    void setDataLength(unsigned int val) throw(SampleLengthException)
    {
	if (val > getAllocLength())
	    throw SampleLengthException(
	    	"SampleT::setDataLength:",val,getAllocLength());
	_header.setDataByteLength(val * sizeof(DataT));
    }

    /**
     * Maximum number of elements in data.
     */
    static unsigned int getMaxDataLength()
    {
    	return SampleHeader::getMaxDataLength() / sizeof(DataT);
    }

    void* getVoidDataPtr() { return (void*) _data; }
    const void* getConstVoidDataPtr() const { return (const void*) _data; }

    DataT* getDataPtr() { return _data; }

    const DataT* getConstDataPtr() const { return _data; }

    /**
     * Implementation of virtual method.
     */
    double getDataValue(unsigned int i) const 
    {
        return (double)_data[i];
    }

    /**
     * Implementation of virtual method.
     */
    void setDataValue(unsigned int i, double val)
    {
        _data[i] = (DataT)val;
    }

    /**
     * Implementation of virtual method.
     */
    void setDataValue(unsigned int i, float val)
    {
        _data[i] = (DataT)val;
    }

    /**
     * Get number of elements allocated in data portion of sample.
     */
    unsigned int getAllocLength() const { return _allocLen / sizeof(DataT); }

    /**
     * Get number of bytes allocated in data portion of sample.
     */
    unsigned int getAllocByteLength() const { return _allocLen; }

    /**
     * Allocate data.  
     * @param val: number of DataT's to allocated.
     */
    void allocateData(unsigned int val) throw(SampleLengthException) {
	if (val  > getMaxDataLength())
	    throw SampleLengthException(
	    	"SampleT::allocateData:",val,getMaxDataLength());
	if (_allocLen < val * sizeof(DataT)) {
	  delete [] _data;
	  _data = new DataT[val];
	  _allocLen = val * sizeof(DataT);
	  setDataLength(0);
	}
    }

    /**
     * Re-allocate data, space, keeping contents.
     * @param val: number of DataT's to allocated.
     */
    void reallocateData(unsigned int val) throw(SampleLengthException) {
	if (val  > getMaxDataLength())
	    throw SampleLengthException(
	    	"SampleT::reallocateData:",val,getMaxDataLength());
	if (_allocLen < val * sizeof(DataT)) {
	  DataT* newdata = new DataT[val];
      std::memcpy(newdata,_data,_allocLen);
	  delete [] _data;
	  _data = newdata;
	  _allocLen = val * sizeof(DataT);
	}
    }

    static int sizeofDataType() { return sizeof(DataT); }

    /**
    * Decrement the reference count for this sample.
    * If the reference count is zero, then put the Sample
    * into the SamplePool.
    * freeReference() can be performed on a const Sample.
    * However if at any moment a user of a const Sample has called
    * freeReference more times than they have called holdReference,
    * then they have violated the contract of a const Sample.
    * Once the sample goes back into the sample pool it may be altered.
    *
    * Samples can be used like normal variables without reference
    * counting:
    *  {
    *	SampleT< samp;		// automatic variable
    *	samp.setTimeTag(99);
    *    ...
      }				// automatic variable destroyed
    *    
    * Or the reference count capability can be used.  Here one
    * saves samples into a buffer, and then later
    * pops them off.
    *
    *   void pushSample(const Sample* samp) 
    *   {
    *     samp->holdReference();
    *     buffer.push_back(samp);
    *   }
    *   const Sample* popSample()
    *   {
    *     const Sample* samp = buffer.back();
    *     buffer.pop_back();
    *     samp->freeReference();
    *     return samp;
    *   }
    *
    *   // Get sample from SamplePool.
    *   // The reference count will be one after the sample
    *   // is fetched from the pool - holdReference is
    *   // called for you by SamplePool.
    *   Sample* samp =
    *	  SamplePool::getInstance()->getSample(100);
    *   ...
    *   pushSample(samp);
    *   ...
    *   samp = popSample();
    *   ...
    *   // When you're completely done with it, call freeReference().
    *   samp->freeReference();
    */
    void freeReference() const;

protected:

    /**
     * Pointer to the actual data.
     */
    DataT* _data;

    /**
     * Number of bytes allocated in data.
     */
    unsigned int _allocLen;

    /** No copy */
    SampleT(const SampleT&);

    /** No assignment */
    SampleT& operator=(const SampleT&);
};

/**
 * A convienence method for getting a sample of an
 * enumerated type from a pool.
 * Returns NULL if type is unknown or len is out of range.
 */
Sample* getSample(sampleType type, unsigned int len);

}}	// namespace nidas namespace core

// Here we define methods which use both the SampleT and SamplePool class.
// We wait until now to include SamplePool.h since it needs to have 
// the SampleT class defined. We must exit the dsm namespace before
// including SamplePool.h

#include <nidas/core/SamplePool.h>

namespace nidas { namespace core {

/**
 * A convenience function for getting a typed sample from a pool.
 */
template <class T>
SampleT<T>* getSample(unsigned int len) throw(SampleLengthException)
{
    SampleT<T>* samp =
    	SamplePool<SampleT<T> >::getInstance()->getSample(len);
    return samp;
}

/**
 * Free a reference to a sample. Return it to its pool if
 * no-one is using it.
 */
template <class DataT>
void SampleT<DataT>::freeReference() const
{
    // if refCount is 0, put it back in the Pool.
#ifdef USE_ATOMIC_REF_COUNT
    // GCC 4.X atomic operations
    int rc = __sync_sub_and_fetch(&_refCount,1);
    assert(rc >= 0);
    if (rc == 0)
	SamplePool<SampleT<DataT> >::getInstance()->putSample(this);
#else
#ifdef MUTEX_PROTECT_REF_COUNTS
    _refLock.lock();
#endif
    bool ref0 = --_refCount == 0;
    assert(_refCount >= 0);
#ifdef MUTEX_PROTECT_REF_COUNTS
    _refLock.unlock();
#endif
    if (ref0)
	SamplePool<SampleT<DataT> >::getInstance()->putSample(this);
#endif
}

}}	// namespace nidas namespace core

#endif
