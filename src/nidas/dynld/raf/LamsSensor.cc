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
#include <nidas/core/DSMEngine.h>
#include <nidas/core/RTL_IODevice.h>
#include <nidas/core/UnixIODevice.h>
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
// DEBUG LOG stuff...
    // number of data values in this raw sample.
    unsigned int nvalues = samp->getDataByteLength() / sizeof(short);

    n_u::Logger::getInstance()->log(LOG_ERR,"LamsSensor::process");
    n_u::Logger::getInstance()->log(LOG_ERR," LamsSensor::process");
    n_u::Logger::getInstance()->log(LOG_ERR,"  LamsSensor::process");
    
//    n_u::Logger::getInstance()->log(LOG_ERR,
//            "LAMS sample id %d (dsm=%d,sensor=%d): Expected %d raw values, got %d",
//            samp->getId(),GET_DSM_ID(samp->getId()),GET_SHORT_ID(samp->getId()),
//            500,nvalues);
// END DEBUG LOG stuff...

/* DSMArincSensor::process does this:
    SampleT<float>* outs = getSample<float>(1);

    // pSamp[i].time is the number of milliseconds since midnight
    // for the individual label. Use it to create a correct
    // time tag for the label, which is in units of microseconds.
    tt = t0day + (dsm_time_t)pSamp[i].time * USECS_PER_MSEC;

    // correct for problems around midnight rollover
    if (::llabs(tt - samp->getTimeTag()) > USECS_PER_HALF_DAY) {
        if (tt > samp->getTimeTag()) tt -= USECS_PER_DAY;
        else tt += USECS_PER_DAY;
    }
    outs->setTimeTag(tt);

    // set the sample id to sum of sensor id and label
    outs->setId( getId() + label );
    outs->setDataLength(1);
    float* d = outs->getDataPtr();

    d[0] = processLabel(pSamp[i].data);
*/
/* IRIG sensor does this:
    dsm_time_t sampt = getTime(samp);

    SampleT<dsm_time_t>* clksamp = getSample<dsm_time_t>(1);
    clksamp->setTimeTag(samp->getTimeTag());
    clksamp->setId(sampleId);
    clksamp->getDataPtr()[0] = sampt;

    result.push_back(clksamp);
    return true;
*/
    SampleT<short>* outs = getSample<short>(1);
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId()+1);
    
    outs->setDataLength(1);
    outs->getDataPtr()[0]= 37;
    //float* dout = outs->getDataPtr();
    //    dout[0] = 42.0;
    //  *dout++ = 35.9;

    results.push_back(outs);
    return true;
} 

void LamsSensor::open(int flags) throw(n_u::IOException,
    n_u::InvalidParameterException)
{

    DSMSensor::open(flags);

    // Request that fifo be opened at driver end.
    if (DSMEngine::getInstance()){
      ioctl(LAMS_OPEN,0,0);
      ioctl(AIR_SPEED, 0,0);
    }
    n_u::Logger::getInstance()->log(LOG_ERR,"LamsSensor::open");
    n_u::Logger::getInstance()->log(LOG_ERR," LamsSensor::open");
    n_u::Logger::getInstance()->log(LOG_ERR,"  LamsSensor::open");
}

void LamsSensor::close() throw(n_u::IOException)
{
    if (DSMEngine::getInstance()->isRTLinux())
	ioctl(LAMS_CLOSE,0,0);
    DSMSensor::close();
}



 IODevice* LamsSensor::buildIODevice() throw(n_u::IOException)
  {
    //return new RTL_IODevice();
    if (DSMEngine::getInstance()->isRTLinux())
        return new RTL_IODevice();
    else return new UnixIODevice();
    
  }
  
  
  