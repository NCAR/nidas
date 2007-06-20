/* 
  LamsSensor
  Copyright 2007 UCAR, NCAR, All Rights Reserved
 
   Revisions:
     $LastChangedRevision:  $
     $LastChangedDate:  $
     $LastChangedBy:  $
     $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynid/LamsSensor.cc $
*/


#include <nidas/dynld/raf/LamsSensor.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/Site.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

using namespace std;
using namespace nidas::core;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,LamsSensor)

bool LamsSensor::process(const Sample* samp,list<const Sample*>& results) throw()
{
    SampleT<float>* outs = getSample<float>(1);

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);

    float* dout = outs->getDataPtr();



    *dout++ = 35.9;



    results.push_back(outs);
    return true;
} 
