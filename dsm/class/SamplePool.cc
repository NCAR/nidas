/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SamplePool.h>

#include <iostream>

using namespace atdUtil;
using namespace std;
using namespace dsm;

/* static */
SamplePool* SamplePool::instance = new SamplePool();

SamplePool* SamplePool::getInstance() { return instance; }

SamplePool::SamplePool():
	smallSize(512),mediumSize(128),bigSize(8),
	nsmall(0),nmedium(0),nbig(0)
{
  smallSamples = new SmallCharSample*[smallSize];
  mediumSamples = new SmallCharSample*[mediumSize];
  bigSamples = new LargeCharSample*[bigSize];
}

SamplePool::~SamplePool() {
  int i;
  for (i = 0; i < nsmall; i++) delete smallSamples[i];
  delete [] smallSamples;
  for (i = 0; i < nmedium; i++) delete mediumSamples[i];
  delete [] mediumSamples;
  for (i = 0; i < nbig; i++) delete bigSamples[i];
  delete [] bigSamples;
}

Sample* SamplePool::getSample(size_t len) {
  Synchronized pooler(poolLock);

#ifdef DEBUG
  cerr << "smallSamples.size=" << smallSamples.size() <<
    " bigSamples.size=" << bigSamples.size() << endl;
#endif

  if (len < 20) return getSample((Sample**)smallSamples,&nsmall,len);
  else if (len < SmallCharSample::getMaxDataLength())
  	return getSample((Sample**)smallSamples,&nsmall,len);
  else return getSample((Sample**)bigSamples,&nbig,len);
}

Sample* SamplePool::getSample(Sample** vec,
    	int *n, size_t len) throw(SampleLengthException) {

    Sample *sample;
    int i = *n - 1;

    if (i >= 0) {
      sample = vec[i];
      if (sample->getAllocLen() < len) sample->allocateData(len);
      *n = i;
      sample->holdReference();
      return sample;
    }

    if (len < SmallCharSample::getMaxDataLength())
	sample = new SmallCharSample();
    else
	sample = new LargeCharSample();

    sample->allocateData(len);
    return sample;

}

void SamplePool::putSample(Sample *sample) {
  Synchronized pooler(poolLock);
  size_t len = sample->getAllocLen();
  if (len < 20)
    	putSample(sample,(Sample***)&smallSamples,&nsmall,&smallSize);
  else if (len < SmallCharSample::getMaxDataLength())
    	putSample(sample,(Sample***)&mediumSamples,&nmedium,&mediumSize);
  else putSample(sample,(Sample***)&bigSamples,&nbig,&bigSize);
}

void SamplePool::putSample(Sample *sample,
    	Sample ***vec,int *n, int *nalloc) {
  if (*n == *nalloc) {
    // cerr << "reallocing, n=" << *n << " nalloc=" << *nalloc << endl;
    // increase by 50%
    int newalloc = *nalloc + (*nalloc >> 1);
    Sample **newvec = new Sample*[newalloc];
    ::memcpy(newvec,*vec,*nalloc * sizeof(Sample*));
    delete [] *vec;
    *vec = newvec;
    *nalloc = newalloc;
  }

  (*vec)[(*n)++] = sample;
}
