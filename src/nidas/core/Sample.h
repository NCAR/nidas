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
#include <nidas/linux/types.h>

#include <climits>
#include <iostream>

#include <cmath>

namespace nidas { namespace core {

/**
 * Value of a float NAN for general use.
 */
extern const float floatNAN;

/** Microseconds since Jan 1 1970, 00:00 UTC */
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
inline size_t maxValue(unsigned short arg)
{
    return USHRT_MAX;
}

inline size_t maxValue(short arg)
{
    return SHRT_MAX;
}

inline size_t maxValue(int arg)
{
    return INT_MAX;
}

inline size_t maxValue(unsigned int arg)
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
inline sampleType getSampleType(char* ptr)
{
    return CHAR_ST;
}

inline sampleType getSampleType(unsigned char* ptr)
{
    return UCHAR_ST;
}

inline sampleType getSampleType(unsigned short* ptr)
{
    return USHORT_ST;
}

inline sampleType getSampleType(short* ptr)
{
    return SHORT_ST;
}

inline sampleType getSampleType(unsigned int* ptr)
{
    return UINT32_ST;
}

inline sampleType getSampleType(int* ptr)
{
    return INT32_ST;
}

inline sampleType getSampleType(float* ptr)
{
    return FLOAT_ST;
}

inline sampleType getSampleType(double* ptr)
{
    return DOUBLE_ST;
}

inline sampleType getSampleType(long long* ptr)
{
    return INT64_ST;
}

inline sampleType getSampleType(void* ptr)
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
    	tt(0),length(0),tid((unsigned int)t << 26) {}

    dsm_time_t getTimeTag() const { return tt; }

    void setTimeTag(dsm_time_t val) { tt = val; }

    /**
     * Get the value of the length member of the header. This
     * is the length in bytes of the data portion of the sample.
     */
    size_t getDataByteLength() const { return length; }

    /**
     * Set the length member of the header. This is the length
     * in bytes.
     */
    void setDataByteLength(size_t val) { length = val; }

    dsm_sample_id_t getId() const { return GET_FULL_ID(tid); }
    void setId(dsm_sample_id_t val) { tid = SET_FULL_ID(tid,val); }

    dsm_sample_id_t getRawId() const { return tid; }
    void setRawId(dsm_sample_id_t val) { tid = val; }

    /**
     * Get the DSM identifier for the sample.
     */
    unsigned int getDSMId() const { return GET_DSM_ID(tid); }
    void setDSMId(unsigned int val) { tid = SET_DSM_ID(tid,val); }

    /**
     * Get the sample identifier for the sample.
     */
    unsigned int getShortId() const { return GET_SHORT_ID(tid); }
    void setShortId(unsigned int val) { tid = SET_SHORT_ID(tid,val); }

    unsigned int getSpSId() const { return GET_SPS_ID(tid); }
    void setSpSId(unsigned int val) { tid = SET_SPS_ID(tid,val); }

    /**
     * Get the data type of this sample.
     */
    unsigned char getType() const { return GET_SAMPLE_TYPE(tid); }

    // void setType(unsigned char val) { tid = SET_SAMPLE_TYPE(tid,val); }

    static size_t getSizeOf()
    {
        return sizeof(dsm_time_t) + sizeof(dsm_sample_length_t) +
		sizeof(dsm_sample_id_t); }

    static size_t getMaxDataLength() { return maxValue(dsm_sample_length_t()); }

protected:

    /**
     * Time-tag in microseconds since Jan 1, 1970 00:00 GMT.
     */
    dsm_time_t tt; 

    /**
     * Length of data (# of bytes) in the sample - does not include
     * header fields
     */
    dsm_sample_length_t length;

    /**
     * An identifier for this sample consisting of packed bit fields.
     * The most significant 6 bits are a data type enumeration
     * (float, double etc), which is accessed with set/getType().
     *
     * The other 26 bits are the sample identifier, which is further
     * broken into 10 bits of a DSM identifier, acccessed with get/setDSMId(),
     * and 16 bits of a sensor/sample identifier, accessed with
     * get/setShortId().
     */
    dsm_sample_id_t tid;
};

/**
 * Interface to a sample of raw data.  A Sample contains
 * a SampleHeader and some data.
 */
class Sample {
public:
  
    Sample(sampleType t = CHAR_ST) : header(t),refCount(1) { nsamps++; }

    virtual ~Sample() { nsamps--; }

    void setTimeTag(dsm_time_t val) { header.setTimeTag(val); }

    dsm_time_t getTimeTag() const { return header.getTimeTag(); }

    /**
     * Set the id portion of the sample header. The id 
     * typically identifies the data system and
     * sensor of origin of the sample.
     */
    void setId(dsm_sample_id_t val) { header.setId(val); }

    /**
     * Get the id portion of the sample header.
     */
    dsm_sample_id_t getId() const { return header.getId(); }

    /**
     * Set the full, raw id portion of the sample header.
     * This method is not typically used except when
     * doing raw IO on a sample. The raw id
     * contains an enumeration of the sample type,
     * along with the fields returned by getId().
     */
    void setRawId(dsm_sample_id_t val) { header.setRawId(val); }

    dsm_sample_id_t getRawId() const { return header.getRawId(); }

    /**
     * Set the short id portion of the sample header.
     * This is the portion of the id without the DSM id.
     */
    void setShortId(unsigned int val) { header.setShortId(val); }

    /**
     * Get the short id portion of the sample header.
     * This is the portion of the id without the DSM id.
     */
    unsigned int getShortId() const { return header.getShortId(); }

    /**
     * Set the DSM (data system) id portion of the sample header.
     */
    void setDSMId(unsigned int val) { header.setDSMId(val); }

    /**
     * Get the DSM (data system) id portion of the sample header.
     */
    unsigned int getDSMId() const { return header.getDSMId(); }

    /**
     * Get the number of bytes in data portion of sample.
     */
    size_t getDataByteLength() const { return header.getDataByteLength(); }

    /**
     * Set the number of elements in data portion of sample.
     */
    virtual void setDataLength(size_t val)
    	throw(SampleLengthException) = 0;
 
    /**
     * Get the number of elements in data portion of sample.
     */
    virtual size_t getDataLength() const = 0;

    /**
     * Get the type of the sample.
     */
    virtual sampleType getType() const = 0;

    /**
     * Number of bytes in header.
     */
    size_t getHeaderLength() const { return SampleHeader::getSizeOf(); }

    /**
     * Get a pointer to the header portion of the sample.
     */
    const void* getHeaderPtr() const { return &header; }

    /**
     * Get a void* pointer to the data portion of the sample.
     */
    virtual void* getVoidDataPtr() = 0;

    /**
     * Get a const void* pointer to the data portion of the sample.
     */
    virtual const void* getConstVoidDataPtr() const = 0;

    /**
     * Get number of elements allocated in data portion of sample.
     */
    virtual size_t getAllocLength() const = 0;

    /**
     * Get number of bytes allocated in data portion of sample.
     */
    virtual size_t getAllocByteLength() const = 0;

    /**
     * Allocate a number of bytes of data.
     */
    virtual void allocateData(size_t val) throw(SampleLengthException) = 0;

    /**
     * Re-allocate a number of bytes of data, saving old contents.
     */
    virtual void reallocateData(size_t val) throw(SampleLengthException) = 0;

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
        __sync_add_and_fetch(&refCount,1);
#else
#ifdef MUTEX_PROTECT_REF_COUNTS
	refLock.lock();
#endif
        refCount++;
#ifdef MUTEX_PROTECT_REF_COUNTS
	refLock.unlock();
#endif
#endif
    }

    /**
     * Decrement the reference count for this sample.
     */
    virtual void freeReference() const = 0;

protected:

    SampleHeader header;

    /**
     * The reference count.
     */
    mutable volatile int refCount;

#ifdef MUTEX_PROTECT_REF_COUNTS
    mutable nidas::util::Mutex refLock;
#endif

    /**
     * Global count of the number of samples in use by a process.
     * Incremented in the constructor, decremented in the destructor.
     * Useful for development debugging to track leaks.
     */
    static int nsamps;
};

/**
 * A typed Sample, with data of type DataT.
 */
template <class DataT>
class SampleT : public Sample {
public:

    SampleT() : Sample(getType()),data(0),allocLen(0) {}

    ~SampleT() { delete [] data; }

    sampleType getType() const { return getSampleType(data); }

    /**
     * Get number of elements of type DataT in data.
     */
    size_t getDataLength() const
    {
        return getDataByteLength() / sizeof(DataT);
    }

    /**
     * Set the number of elements of type DataT in data.
     * @param val: number of elements.
     */
    void setDataLength(size_t val) throw(SampleLengthException)
    {
	if (val > getAllocLength())
	    throw SampleLengthException(
	    	"SampleT::setDataLength:",val,getAllocLength());
	header.setDataByteLength(val * sizeof(DataT));
    }

    /**
     * Maximum number of elements in data.
     */
    static size_t getMaxDataLength()
    {
    	return SampleHeader::getMaxDataLength() / sizeof(DataT);
    }

    void* getVoidDataPtr() { return (void*) data; }
    const void* getConstVoidDataPtr() const { return (const void*) data; }

    DataT* getDataPtr() { return data; }

    const DataT* getConstDataPtr() const { return data; }

    /**
     * Get number of elements allocated in data portion of sample.
     */
    size_t getAllocLength() const { return allocLen / sizeof(DataT); }

    /**
     * Get number of bytes allocated in data portion of sample.
     */
    size_t getAllocByteLength() const { return allocLen; }

    /**
     * Allocate data.  
     * @param val: number of DataT's to allocated.
     */
    void allocateData(size_t val) throw(SampleLengthException) {
	if (val  > getMaxDataLength())
	    throw SampleLengthException(
	    	"SampleT::allocateData:",val,getMaxDataLength());
	if (allocLen < val * sizeof(DataT)) {
	  delete [] data;
	  data = new DataT[val];
	  allocLen = val * sizeof(DataT);
	  setDataLength(0);
	}
    }

    /**
     * Re-allocate data, space, keeping contents.
     * @param val: number of DataT's to allocated.
     */
    void reallocateData(size_t val) throw(SampleLengthException) {
	if (val  > getMaxDataLength())
	    throw SampleLengthException(
	    	"SampleT::allocateData:",val,getMaxDataLength());
	if (allocLen < val * sizeof(DataT)) {
	  DataT* newdata = new DataT[val];
	  memcpy(newdata,data,allocLen);
	  delete [] data;
	  data = newdata;
	  allocLen = val * sizeof(DataT);
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
    *	  SamplePool::getReference()->getSample(100);
    *   ...
    *   pushSample(samp);
    *   ...
    *   samp = popSample();
    *   ...
    *   // When you're done with it, call freeReference().
    *   samp->freeReference();
    */
    void freeReference() const;

protected:

    /**
     * Pointer to the actual data.
     */
    DataT* data;

    /**
     * Number of bytes allocated in data.
     */
    size_t allocLen;
};

/**
 * A convienence method for getting a sample of an
 * enumerated type from a pool.
 * Returns NULL if type is unknown or len is out of range.
 */
Sample* getSample(sampleType type, size_t len);

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
SampleT<T>* getSample(size_t len) throw(SampleLengthException)
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
    int rc = __sync_sub_and_fetch(&refCount,1);
    assert(rc >= 0);
    if (rc == 0)
	SamplePool<SampleT<DataT> >::getInstance()->putSample(this);
#else
#ifdef MUTEX_PROTECT_REF_COUNTS
    refLock.lock();
#endif
    bool ref0 = --refCount == 0;
    assert(refCount >= 0);
#ifdef MUTEX_PROTECT_REF_COUNTS
    refLock.unlock();
#endif
    if (ref0)
	SamplePool<SampleT<DataT> >::getInstance()->putSample(this);
#endif
}

}}	// namespace nidas namespace core

#endif
