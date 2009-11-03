/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/CVI_LV_Input.cc $

*/

#include <nidas/dynld/raf/CVI_LV_Input.h>
#include <nidas/core/ServerSocketIODevice.h>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,CVI_LV_Input)

CVI_LV_Input::CVI_LV_Input(): _tt0(0) 
{
}

void CVI_LV_Input::open(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    CharacterSensor::open(flags);
    init();
}

IODevice* CVI_LV_Input::buildIODevice() throw(n_u::IOException)
{
    ServerSocketIODevice* dev = new ServerSocketIODevice();
    dev->setTcpNoDelay(true);
    return dev;
}

bool CVI_LV_Input::process(const Sample * samp,
    list < const Sample * >&results) throw()
{

    // invoke the standard scanf processing.
    list<const Sample*> tmpresults;
    CharacterSensor::process(samp, tmpresults);

    list<const Sample *>::const_iterator it = tmpresults.begin();
    for (; it != tmpresults.end(); ++it) {
        samp = *it;
        if (samp->getType() != FLOAT_ST) {   // shouldn't happen
            samp->freeReference();
            continue;
        }

        const SampleT<float>* fsamp = static_cast<const SampleT<float>*>(samp);

        int nd = fsamp->getDataLength();
        if (nd < 2) {
            samp->freeReference();
            continue;
        }

        dsm_time_t tt = fsamp->getTimeTag();

        const float* fptr = fsamp->getConstDataPtr();

        // seconds of day timetag echoed back by LabView
        double ttback = fptr[0];

        dsm_time_t tnew = _tt0 + (dsm_time_t)(ttback * USECS_PER_SEC);

        if (::llabs(tnew - tt) > USECS_PER_HALF_DAY) {
            _tt0 = tt - (tt % USECS_PER_DAY);
            tnew = _tt0 + (dsm_time_t)(ttback * USECS_PER_SEC);
            // correct for possibility that we could be off by a day here,
            // either the dsm or dsm_server gets restarted after 00:00 UTC
            if (tnew > tt + USECS_PER_HALF_DAY) _tt0 -= USECS_PER_DAY;
            else if (tnew < tt - USECS_PER_HALF_DAY) _tt0 += USECS_PER_DAY;
            tnew = _tt0 + (dsm_time_t)(ttback * USECS_PER_SEC);
        }

        SampleT<float>* outs = getSample<float>(nd);
        outs->setTimeTag(tnew);
        outs->setId(fsamp->getId());
        float* fptr2 = outs->getDataPtr();
        *fptr2++ = (float)(tt - tnew) / USECS_PER_SEC;
        for (int i = 1; i < nd; i++) *fptr2++ = fptr[i];
        results.push_back(outs);
        samp->freeReference();
    }
    return results.size() > 0;
}
