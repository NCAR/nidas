/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3654 $

    $LastChangedDate: 2007-02-01 14:40:14 -0700 (Thu, 01 Feb 2007) $

    $LastChangedRevision: 3654 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoD_House.cc $

*/

#include <nidas/dynld/raf/TwoD_House.h>
#include <nidas/util/Logger.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,TwoD_House)


TwoD_House::TwoD_House() : DSMSerialSensor(), _noutValues(7)
{
  ::memset(_houseKeeping, 0, sizeof(_houseKeeping));
}


bool TwoD_House::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    const char * input = (char *) samp->getConstVoidDataPtr();

    SampleT<float> * outs = getSample<float>(_noutValues);
    float * dout = outs->getDataPtr();
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);

    unsigned int value[8];
    sscanf(input, "%x %x %x %x %x %x %x %x",
	&value[0], &value[1], &value[2], &value[3], &value[4], &value[5], &value[6], &value[7]);

    float shadow_or = 0.0;
    for (int iout = 0; iout < 5; ++iout)
        shadow_or += value[iout];

// Get mapping from Spowart for each housekeeping that comes and stash into _houseKeep array.

    // Push the housekeeping.  There are not all decoded/available every sample,
    // they come round robin.
    for (int iout = 0; iout < 6; ++iout)
        *dout++ = _houseKeeping[iout];

    *dout++ = shadow_or;

    results.push_back(outs);
    return true;
}
