/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $

    Some over-engineered sample classes.
 ********************************************************************

*/

#ifndef DSM_SAMPLET_H
#define DSM_SAMPLET_H

#include <SampleLengthException.h>
#include <dsm_sample.h>

#include <limits.h>
#include <iostream>

namespace dsm {

#define CLOCK_SAMPLE_ID 1

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

inline size_t maxValue(long arg)
{
    return LONG_MAX;
}

inline size_t maxValue(unsigned long arg)
{
    return ULONG_MAX;
}

typedef enum sampleType {
	CHAR_ST, UCHAR_ST, SHORT_ST, USHORT_ST,
	LONG_ST, ULONG_ST, FLOAT_ST, DOUBLE_ST,
	LONG_LONG_ST, UNKNOWN_ST } sampleType;

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

inline sampleType getSampleType(unsigned long* ptr)
{
    return ULONG_ST;
}

inline sampleType getSampleType(long* ptr)
{
    return LONG_ST;
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
    return LONG_LONG_ST;
}

inline sampleType getSampleType(void* ptr)
{
    return UNKNOWN_ST;
}

/**
 * Interface to a sample of raw data.  A sample contains
 * a time_tag, a data length field, a sensor id value, and some data.
 */
class Sample {
public:
  
    virtual ~Sample() {}

    virtual void setTimeTag(dsm_sample_time_t val) = 0;
    virtual dsm_sample_time_t getTimeTag() const = 0;

    virtual const Sample* getNext() const = 0;
    virtual void setNext(const Sample* val) = 0;

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
     * Get the number of bytes in data portion of sample.
     */
    virtual size_t getDataByteLength() const = 0;

    /**
     * Set the id portion of the sample header. The id can
     * identify the sensor of origin of the sample.
     */
    virtual void setId(short val) = 0;

    /**
     * Set the id portion of the sample header.
     */
    virtual short getId() const = 0;

    /**
     * Set the type of the sample.
     */
    virtual sampleType getType() const = 0;

    /**
     * Number of bytes in header.
     */
    virtual size_t getHeaderLength() const = 0;

    /**
     * Get a pointer to the header portion of the sample.
     */
    virtual const void* getHeaderPtr() const = 0;

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
     * Increment the reference count for this sample.
     * Sample supports a form of reference counting.
     */
    virtual void holdReference() const = 0;

    /**
     * Decrement the reference count for this sample.
     */
    virtual void freeReference() const = 0;

    virtual void freeReferencesOfList() const = 0;

};


class SampleBase : public Sample {

public:
    SampleBase() : refCount(1),next(0) { nsamps++; }
    virtual ~SampleBase() { nsamps--; }

    virtual const Sample* getNext() const { return next; }

    virtual void setNext(const Sample* val) { next = val; }

    /**
    * Increment the reference count for this sample.
    * Sample supports a form of reference counting.
    * See freeReference.  We have to use a hack-like
    * cast of the this pointer to a const so that holdReference
    * can be used on a const Sample.  The SamplePool class
    * supports Sample reference counting.
    */
    void holdReference() const { refCount++; }

    void freeReferencesOfList() const
    {
	for (const Sample* samp = this; samp; ) {
	    const Sample* nxt = samp->getNext();
	    samp->freeReference();
	    samp = nxt;
	}
    }

protected:

  /**
   * The reference count.
   */
  mutable int refCount;

  const Sample* next;

  /**
   * Global count of the number of samples in use by a process.
   * Incremented in the constructor, decremented in the destructor.
   * Useful for development debugging to track leaks.
   */
  static int nsamps;

};

/**
 * The header fields of a Sample.
 */
class SampleHeader {
public:

    SampleHeader(sampleType t=CHAR_ST) :
    	tt(0),length(0),id(-1),type((short)t) {}

    typedef short dsm_sample_id_t;

    dsm_sample_time_t getTimeTag() const { return tt; }
    void setTimeTag(dsm_sample_time_t val) { tt = val; }

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

    short getId() const { return id; }
    void setId(short val) { id = val; }

    short getType() const { return type; }
    void setType(short val) { type = val; }

    static size_t getSizeOf()
    {
        return sizeof(dsm_sample_time_t) + sizeof(dsm_sample_length_t) +
		sizeof(dsm_sample_id_t); }

    static size_t getMaxDataLength() { return maxValue(dsm_sample_length_t()); }

protected:

    /* Time-tag. By convention, milliseconds since midnight 00:00 GMT */
    dsm_sample_time_t tt; 

    /* Length of data (# of bytes) in the sample - does not include
     * header fields */
    dsm_sample_length_t length;

    /* An identifier for this sample - which sensor did it come from */
    dsm_sample_id_t id;

    /* type of this sample */
    short type;
};

/**
 * A typed Sample, with data of type DataT.
 */
template <class DataT>
class SampleT : public SampleBase {
public:
    SampleT() : SampleBase(),header(getType()),data(0),allocLen(0) {}
    ~SampleT() { delete [] data; }

    void setTimeTag(dsm_sample_time_t val) { header.setTimeTag(val); }
    dsm_sample_time_t getTimeTag() const { return header.getTimeTag(); }

    void setId(short val) { header.setId(val); }
    short getId() const { return header.getId(); }

    sampleType getType() const { return getSampleType(data); }
    /**
     * Get number of elements of type DataT in data.
     */
    size_t getDataLength() const
    {
        return header.getDataByteLength() / sizeof(DataT);
    }

    /**
     * Get number of bytes in data.
     */
    size_t getDataByteLength() const { return header.getDataByteLength(); }

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

    /**
     * Get number of bytes in header portion of sample.
     */
    size_t getHeaderLength() const { return SampleHeader::getSizeOf(); }

    const void* getHeaderPtr() const { return (void*) &header; }

    void* getVoidDataPtr() { return (void*) data; }
    const void* getConstVoidDataPtr() const { return (const void*) data; }

    DataT* getDataPtr() { return data; }

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

    SampleHeader header;

    /**
     * Pointer to the actual data.
     */
    DataT* data;

    /**
     * Number of bytes allocated in data.
     */
    size_t allocLen;
};

/* typedefs for common sample types. */

/**
 * A Sample with an array of chars for data.
 */
typedef SampleT<char> CharSample;

/**
 * A Sample with an array of shorts for data.
 */
typedef SampleT<short> ShortIntSample;

/**
 * A Sample with an array of unsigned shorts for data.
 */
typedef SampleT<unsigned short> UnsignedShortIntSample;

/**
 * A Sample with an array of floats for data.
 */
typedef SampleT<float> FloatSample;

/**
 * A Sample with an array of long longs for data.  One use of
 * this is to store an absolute time - milliseconds since Jan 1 1970.
 */
typedef SampleT<long long> LongLongSample;

/**
 * A convienence method for getting a sample of an
 * enumerated type from a pool.
 */
Sample* getSample(sampleType type, size_t len);

}

// Here we define methods which use both the SampleT and SamplePool class.
// We wait until now to include SamplePool.h since it needs to have 
// the SampleT class defined. We must exit the dsm namespace before
// including SamplePool.h

#include <SamplePool.h>

namespace dsm {

/**
 * A convienence function for getting a typed sample from a pool.
 */
template <class T>
SampleT<T>* getSample(size_t len)
{
    SampleT<T>* samp =
    	SamplePool<SampleT<T> >::getInstance()->getSample(len);
    samp->setDataLength(len);
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
    if (! --refCount)
	SamplePool<SampleT<DataT> >::getInstance()->putSample(this);
}

}
#endif
