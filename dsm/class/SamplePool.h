/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_SAMPLEPOOL_H
#define DSM_SAMPLEPOOL_H

#include <atdUtil/ThreadSupport.h>
#include <Sample.h>

#include <vector>

namespace dsm {

/**
 * A pool of Samples.  Actually three pools, containing
 * samples segregated by size.
 */
class SamplePool {

private:
  static SamplePool* instance;

public:
  SamplePool();
  ~SamplePool();

  Sample *getSample(size_t len);
  void putSample(Sample *);

  static SamplePool *getInstance();

protected:
  Sample *getSample(Sample** vec,int *veclen, size_t len)
  	throw(SampleLengthException);
  void putSample(Sample *,Sample*** vecp,int *veclen, int* nalloc);

  SmallCharSample** smallSamples;
  SmallCharSample** mediumSamples;
  LargeCharSample** bigSamples;

  int smallSize;
  int mediumSize;
  int bigSize;

  int nsmall;
  int nmedium;
  int nbig;

  atdUtil::Mutex poolLock;

};
}

#endif
