#include <Sample.h>
#include <SamplePool.h>

using namespace dsm;

/* static */
int SampleBase::nsamps = 0;

void SampleBase::freeReference() const {
    // if refCount is 0, put it back in the Pool.
    if (! --(((SampleBase*)this)->refCount))
	SamplePool::getInstance()->putSample((Sample*)this);
}



