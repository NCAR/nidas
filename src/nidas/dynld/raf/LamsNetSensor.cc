/* 
  LamsNetSensor
  Copyright 2007-2011 UCAR, NCAR, All Rights Reserved
 
   Revisions:
     $LastChangedRevision:  $
     $LastChangedDate:  $
     $LastChangedBy:  $
     $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynid/LamsNetSensor.cc $
*/


#include <nidas/dynld/raf/LamsNetSensor.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/Site.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iostream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,LamsNetSensor)

LamsNetSensor::LamsNetSensor() :
    UDPSocketSensor()
{
  for (int i = 0; i < nBeams; ++i)
    saveSamps[i] = 0;
}

bool LamsNetSensor::process(const Sample* samp,list<const Sample*>& results) throw()
{
    unsigned int len = samp->getDataByteLength();
    const uint32_t *data = (uint32_t *)samp->getConstVoidDataPtr();

    if (len < sizeof(int)) return false;
    uint32_t syncWord = data[0];

    int beam = 0;	// This needs to be based off the sync word

    if (len == 1200)
    {	// First half of data.
        if (saveSamps[beam])
        {
            saveSamps[beam]->freeReference();
            WLOG(("Didn't get second half of record."));
        }
        saveSamps[beam] = samp;
        samp->holdReference();
        return false;
    }
    else	// Second half of data, push it out.
    {
        if (saveSamps[beam] == 0)
        {
            // Log Message, lost data.  Do nothing.
            return false;
        }
        else
        {
            // Don't forget endianConverters.

            // allocate sample
            SampleT<float> * outs = getSample<float>(LAMS_SPECTRA_SIZE);
            outs->setTimeTag(saveSamps[beam]->getTimeTag());
            outs->setId(getId() + 1);  

            // extract data from a lamsPort structure
            float *dout = outs->getDataPtr();
            int iout;

            data = (uint32_t *)saveSamps[beam]->getConstVoidDataPtr();
            for (iout = 2; iout < 368; iout++)
                *dout++ = (float)data[iout];

            data = (uint32_t *)samp->getConstVoidDataPtr();
            for ( ; iout < LAMS_SPECTRA_SIZE; iout++)
                *dout++ = (float)data[iout];

            saveSamps[beam]->freeReference();
            saveSamps[beam] = 0;
            results.push_back(outs);
        }
    }

    return true;
}
