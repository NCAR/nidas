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

// More for documentation than anything.
const size_t TwoD_House::V15_INDX = 0;
const size_t TwoD_House::TMP_INDX = 1;
const size_t TwoD_House::EE1_INDX = 4;
const size_t TwoD_House::EE32_INDX = 5;
const size_t TwoD_House::VN15_INDX = 6;
const size_t TwoD_House::V5_INDX = 7;


TwoD_House::~TwoD_House()
{
}

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

    unsigned int shado[5];
    unsigned int tag;
    unsigned int hkp;
    sscanf(input, "%x %x %x %x %x %x %x",
	&shado[0], &shado[1], &shado[2], &shado[3], &shado[4], &tag, &hkp);

    float shadow_or = 0.0;
    for (int iout = 0; iout < 5; ++iout)
        shadow_or += shado[iout];

    // Push the housekeeping.  They are not all decoded/available every sample,
    // they come round robin.
    if (tag < 8)
       _houseKeeping[tag] = hkp;

    *dout++ = _houseKeeping[V15_INDX];
    *dout++ = _houseKeeping[TMP_INDX];
    *dout++ = _houseKeeping[EE1_INDX] * 0.001;
    *dout++ = _houseKeeping[EE32_INDX] * 0.001;
    *dout++ = _houseKeeping[VN15_INDX];
    *dout++ = _houseKeeping[V5_INDX];

    *dout++ = shadow_or;

    results.push_back(outs);
    return true;
}
