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

#ifndef DSM_RAWSAMPLE_H
#define DSM_RAWSAMPLE_H

#include <string>
#include <SampleLengthException.h>
#include <MaxValue.h>
#include <dsm_sample.h>

namespace dsm {

/**
 * Interface to a sample of raw data.  A sample contains
 * a time_tag, a data length field, a sensor id value, and some data.
 */
class Sample {
public:
  
    // virtual ~Sample() {}

    virtual void setTimeTag(dsm_sample_time_t val) = 0;
    virtual dsm_sample_time_t getTimeTag() const = 0;

    virtual size_t getDataLength() const = 0;
    virtual void setDataLength(size_t val)
    	throw(SampleLengthException) = 0;

    virtual void setId(int val) = 0;
    virtual int getId() const = 0;

    virtual size_t getHeaderLength() const = 0;

    virtual const void* getHeaderPtr() const = 0;

    virtual void* getVoidDataPtr() = 0;
    virtual const void* getConstVoidDataPtr() const = 0;

    virtual size_t getAllocLen() const = 0;
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

};


class SampleBase : public Sample {

public:
    SampleBase() : refCount(1) { nsamps++; }
    virtual ~SampleBase() { nsamps--; }

    /**
    * Increment the reference count for this sample.
    * Sample supports a form of reference counting.
    * See freeReference.  We have to use a hack-like
    * cast of the this pointer to a const so that holdReference
    * can be used on a const Sample.  The SamplePool class
    * supports Sample reference counting.
    */
    void holdReference() const { ((SampleBase*)this)->refCount++; }

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
    *	Sample samp;		// automatic variable
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
    *
    *   pushSample(samp);
    *   ...
    *   samp = popSample();
    *   // When you're done with it, call freeReference. This 
    *   // should decrement the reference count to zero, and
    *   // cause the sample to be returned to the SamplePool.
    *   // If you deal the sample to anyone else, they
    *   // must do a holdReference() for themselves.
    *   samp->freeReference();
    *
    * TODO: write a test program and see if the SamplePool
    * actually speeds things up - is it faster than new()?
    */
    void freeReference() const;

private:

  /**
   * The reference count.
   */
  int refCount;

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
template <class TT,class LT,class IT>
class SampleHeader {
public:
    typedef TT timetag_t;
    typedef LT length_t;
    typedef IT id_t;

    SampleHeader() : tt(0),length(0),id(-1) {}

    TT getTimeTag() const { return tt; }
    void setTimeTag(TT val) { tt = val; }

    size_t getDataLength() const { return length; }
    void setDataLength(size_t val) throw(SampleLengthException)
    {
	if (val > getMaxDataLength())
	    throw SampleLengthException(
	    	"SampleHeader::setDataLength:",val,getMaxDataLength());
        length = val;
    }

    int getId() const { return id; }
    void setId(int val) { id = val; }

    static size_t getSizeOf() { return sizeof(TT) + sizeof(LT) + sizeof(IT); }

    static size_t getMaxDataLength() { return maxValue(LT()); }
protected:

    /* Time-tag. By convention, milliseconds since midnight 00:00 GMT */
    TT tt; 

    /* Length of data (# of bytes) in the sample - does not include
     * header fields */
    LT length;

    /* An identifier for this sample - which sensor did it come from */
    IT id;
};

/**
 * Header for a Sample, with a timetag of type dsm_sample_time_t
 * an unsigned short data length, and a signed short id.
 * The data length of a SmallSampleHeader is therefore limited
 * by the maximum value of an unsigned short (2^16-1=65535).
 * 
 */
class SmallSampleHeader :
    public SampleHeader<dsm_sample_time_t, unsigned short, short>
{
};


/**
 * Header for a Sample, with a timetag of type dsm_sample_time_t
 * an unsigned long data length, and a signed long id.
 */
class LargeSampleHeader :
    public SampleHeader<dsm_sample_time_t, unsigned long, int>
{
};


/**
 * A typed Sample, with a header of type HeaderT and data of type DataT.
 */
template <class HeaderT,class DataT>
class SampleT : public SampleBase {
public:
    SampleT() : SampleBase(),header(),data(0),allocLen(0) {}
    ~SampleT() { delete [] data; }

    // typedef HeaderT::timetag_t timetag_t;
    // typedef HeaderT::length_t length_t;
    // typedef HeaderT::id_t id_t;

    void setTimeTag(dsm_sample_time_t val) { header.setTimeTag(val); }
    dsm_sample_time_t getTimeTag() const { return header.getTimeTag(); }

    void setId(int val) { header.setId(val); }
    int getId() const { return header.getId(); }

    size_t getDataLength() const { return header.getDataLength(); }
    void setDataLength(size_t val) throw(SampleLengthException) {
	if (val > getAllocLen())
	    throw SampleLengthException(
	    	"SampleT::setDataLength:",val,getAllocLen());
	header.setDataLength(val);
    }

    static size_t getMaxDataLength() { return HeaderT::getMaxDataLength(); }
    size_t getHeaderLength() const { return HeaderT::getSizeOf(); }

    const void* getHeaderPtr() const { return (void*) &header; }

    void* getVoidDataPtr() { return (void*) data; }
    const void* getConstVoidDataPtr() const { return (const void*) data; }

    DataT* getDataPtr() { return data; }

    size_t getAllocLen() const { return allocLen; }

    void allocateData(size_t val) throw(SampleLengthException) {
	if (val > getMaxDataLength())
	    throw SampleLengthException(
	    	"SampleT::allocateData:",val,getMaxDataLength());
	if (allocLen < val) {
	  delete [] data;
	  data = new DataT[val];
	  allocLen = val;
	  setDataLength(0);
	}
    }

private:

  HeaderT header;

  DataT* data;

  size_t allocLen;

};

/**
 * A simple Sample with a small header and an array of chars for data.
 */
class SmallCharSample :
    public SampleT<SmallSampleHeader, char>
{
};

/**
 * A simple Sample with a large header and an array of chars for data.
 */
class LargeCharSample :
    public SampleT<LargeSampleHeader, char>
{
};


}

#endif

