/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SAMPLEPOOL_H
#define DSM_SAMPLEPOOL_H

#include <atdUtil/ThreadSupport.h>
#include <SampleLengthException.h>

#include <cassert>
#include <vector>
#include <list>
#include <iostream>

namespace dsm {

class SamplePoolInterface
{
public:
    virtual ~SamplePoolInterface() {}
    virtual int getNSamplesAlloc() const = 0;
    virtual int getNSamplesOut() const = 0;
};

class SamplePools
{
public:
    static SamplePools* getInstance();
    std::list<SamplePoolInterface*> getPools() const;
    void addPool(SamplePoolInterface* pool);
    void removePool(SamplePoolInterface* pool);
private:
    static SamplePools* instance;
    static atdUtil::Mutex instanceLock;
    std::list<SamplePoolInterface*> pools;
};

/**
 * A pool of Samples.  Actually two pools, containing
 * samples segregated by size.  A SamplePool can used
 * as a singleton, and accessed from anywhere, via the
 * getInstance() static member function.
 */
template <typename SampleType>
class SamplePool : public SamplePoolInterface {

private:
    // static SamplePool<SampleType>* instance;
    static SamplePool* instance;
    static atdUtil::Mutex instanceLock;

public:

    SamplePool();
    ~SamplePool();

    /**
     * Get a pointer to the singleton instance.
     */
    static SamplePool *getInstance();

    /**
     * Singleton cleanup on program exit.
     */
    static void deleteInstance();

    /**
     * Get a sample of at least len elements from the pool.
     */
    SampleType *getSample(size_t len);

    /**
     * Return a sample to the pool.
     */
    void putSample(const SampleType *);

    int getNSamplesAlloc() const { return nsamplesAlloc; }

    int getNSamplesOut() const { return nsamplesOut; }

protected:
    SampleType *getSample(SampleType** vec,int *veclen, size_t len)
	throw(SampleLengthException);
    void putSample(const SampleType *,SampleType*** vecp,int *veclen, int* nalloc);

    SampleType** smallSamples;
    SampleType** mediumSamples;

    int smallSize;
    int mediumSize;

    atdUtil::Mutex poolLock;

public:
    int nsmall;
    int nmedium;

    int nsamplesOut;

    int nsamplesAlloc;

};

/* static */
template<class SampleType>
SamplePool<SampleType>* SamplePool<SampleType>::instance = 0;

/* static */
template<class SampleType>
atdUtil::Mutex SamplePool<SampleType>::instanceLock = atdUtil::Mutex();

/* static */
template<class SampleType>
SamplePool<SampleType> *SamplePool<SampleType>::getInstance()
{
    if (!instance) {
	atdUtil::Synchronized pooler(instanceLock);
	if (!instance) {
	    instance = new SamplePool<SampleType>();
	    SamplePools::getInstance()->addPool(instance);
	}
    }
    return instance;
}

/* static */
template<class SampleType>
void SamplePool<SampleType>::deleteInstance()
{
    atdUtil::Synchronized pooler(instanceLock);
    SamplePools::getInstance()->removePool(instance);
    delete instance;
    instance = 0;
}

template<class SampleType>
SamplePool<SampleType>::SamplePool():
    nsmall(0),nmedium(0),nsamplesOut(0),nsamplesAlloc(0)
{
    // Initial size of pool of small samples around 16K bytes
    smallSize = 16384 / (sizeof(SampleType) + 32 * SampleType::sizeofDataType());
    // When we expand the size of the pool, we expand by 50%
    // so minimum size should be at least 2.
    if (smallSize < 2) smallSize = 2;
    mediumSize = smallSize / 4;
    if (mediumSize < 2) mediumSize = 2;

    smallSamples = new SampleType*[smallSize];
    mediumSamples = new SampleType*[mediumSize];
}

template<class SampleType>
SamplePool<SampleType>::~SamplePool() {
    int i;
    for (i = 0; i < nsmall; i++) delete smallSamples[i];
    delete [] smallSamples;
    for (i = 0; i < nmedium; i++) delete mediumSamples[i];
    delete [] mediumSamples;
}

template<class SampleType>
SampleType* SamplePool<SampleType>::getSample(size_t len) {

    atdUtil::Synchronized pooler(poolLock);

    // Shouldn't get back more than I've dealt out
    // If we do, that's an indication that reference counting
    // is screwed up.
    assert(nsamplesOut >= 0);

    // conservation of sample numbers:
    // total number allocated must equal:
    //		number in the pool (nsmall + nmedium), plus
    //		number held by others.
    assert(nsamplesAlloc == nsmall + nmedium + nsamplesOut);

    if (len < 32) {
	if (nsmall == 0 && nmedium > 20)
	    return getSample((SampleType**)mediumSamples,&nmedium,len);
	else
	    return getSample((SampleType**)smallSamples,&nsmall,len);
    }
    else return getSample((SampleType**)mediumSamples,&nmedium,len);
}

template<class SampleType>
SampleType* SamplePool<SampleType>::getSample(SampleType** vec,
    	int *n, size_t len) throw(SampleLengthException) {

    SampleType *sample;
#ifdef DEBUG
    std::cerr << "getSample, this=" << std::hex << this <<
    	" pool=" << vec << std::dec <<
    	" *n=" << *n << std::endl;
#endif
    int i = *n - 1;

    if (i >= 0) {
      sample = vec[i];
      if (sample->getAllocLength() < len) sample->allocateData(len);
      sample->setDataLength(len);
      *n = i;
      sample->holdReference();
      nsamplesOut++;
      return sample;
    }

    sample = new SampleType();
    sample->allocateData(len);
    sample->setDataLength(len);
    nsamplesAlloc++;
    nsamplesOut++;
    return sample;

}

template<class SampleType>
void SamplePool<SampleType>::putSample(const SampleType *sample) {

    atdUtil::Synchronized pooler(poolLock);

    assert(nsamplesOut >= 0);
    assert(nsamplesAlloc == nsmall + nmedium + nsamplesOut);

    size_t len = sample->getAllocLength();
    if (len < 32)
	putSample(sample,(SampleType***)&smallSamples,&nsmall,&smallSize);
    else putSample(sample,(SampleType***)&mediumSamples,&nmedium,&mediumSize);
}

template<class SampleType>
void SamplePool<SampleType>::putSample(const SampleType *sample,
    	SampleType ***vec,int *n, int *nalloc) {
#ifdef DEBUG
    std::cerr << "putSample, this=" << std::hex << this <<
    	" pool=" << *vec << std::dec <<
    	" *n=" << *n << std::endl;
#endif
    // increase by 50%
    if (*n == *nalloc) {
	// cerr << "reallocing, n=" << *n << " nalloc=" << *nalloc << endl;
	// increase by 50%
	int newalloc = *nalloc + (*nalloc >> 1);
	SampleType **newvec = new SampleType*[newalloc];
	::memcpy(newvec,*vec,*nalloc * sizeof(SampleType*));
	delete [] *vec;
	*vec = newvec;
	*nalloc = newalloc;
    }

    (*vec)[(*n)++] = (SampleType*) sample;
    nsamplesOut--;
}

}
#endif
