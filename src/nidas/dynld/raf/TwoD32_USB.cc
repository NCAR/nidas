/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3716 $

    $LastChangedDate: 2007-03-08 13:43:19 -0700 (Thu, 08 Mar 2007) $

    $LastChangedRevision: 3716 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoD32_USB.cc $

 ******************************************************************
*/


#include <nidas/linux/usbtwod/usbtwod.h>
#include <nidas/dynld/raf/TwoD32_USB.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, TwoD32_USB)

const unsigned long TwoD32_USB::_syncWord = 0x55000000L;
const unsigned long TwoD32_USB::_syncMask = 0xff000000L; 


TwoD32_USB::TwoD32_USB()
{
    _syncWordBE = bigEndian->ulongValue(&_syncWord);
    _syncMaskBE = bigEndian->ulongValue(&_syncMask);
    _bitn = 32;
}


TwoD32_USB::~TwoD32_USB()
{
}

bool TwoD32_USB::processImage(const Sample * samp,
                             list < const Sample * >&results) throw()
{

    unsigned long lin = samp->getDataByteLength();
    // first 2 longs are the stype and the TAS structure,
    // then expect 1024 longs.
    if (lin < 2 * sizeof (long) + 1024 * sizeof (long))
        return false;
    const unsigned long *lptr =
        (const unsigned long *) samp->getConstVoidDataPtr();
    int stype = bigEndian->longValue(*lptr++);
    *lptr++;		// skip 4 byte TAS structure

    // We will compute 1 value, a count of particles.
    size_t nvalues = 1;
    SampleT < float >*outs = getSample < float >(nvalues);

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);   //

    float *dout = outs->getDataPtr();

    // Count number of particles (sync words) in the record and return.
    int cnt = 0;
    for (int i = 0; i < 1024; ++i, lptr++) {
        if ((*lptr & _syncMaskBE) == _syncWordBE) cnt++;
    }

    *dout = cnt;
    results.push_back(outs);

    return true;
}

/*---------------------------------------------------------------------------*/

bool TwoD32_USB::process(const Sample * samp,
                        list < const Sample * >&results) throw()
{
    
    if (samp->getDataByteLength() < sizeof (long))
        return false;

    const unsigned long *lptr =
        (const unsigned long *) samp->getConstVoidDataPtr();
    int stype = bigEndian->longValue(*lptr++);

    /* From the usbtwod driver: stype=0 is image data, stype=1 is not used in 32 probe.  */
    if (stype ==0) {
        return processImage(samp, results);
    }
    return false;
}

