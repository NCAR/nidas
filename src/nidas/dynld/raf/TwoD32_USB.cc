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

const unsigned int TwoD32_USB::_syncWord = 0x55000000;
const unsigned int TwoD32_USB::_syncMask = 0xff000000; 
const unsigned char TwoD32_USB::_syncChar = 0x55;


TwoD32_USB::TwoD32_USB()
{
}

TwoD32_USB::~TwoD32_USB()
{
}

bool TwoD32_USB::processImage(const Sample * samp,
                             list < const Sample * >&results) throw()
{
    bool rc = false;    // return code.

    if (samp->getDataByteLength() < 2 * sizeof (int32_t) +
        1024 * sizeof (int32_t)) return rc;

    unsigned long long startTime = _prevTime;
    _prevTime = samp->getTimeTag();
    _totalRecords++;

    if (_nowTime == 0)
    {
        _nowTime = samp->getTimeTag();
        _nowTime -= (_nowTime % USECS_PER_SEC); // nowTime should have no fractional component.
        return rc;      // Chuck first record as we don't know start time.
    }

    const int * dp = (const int *) samp->getConstVoidDataPtr();
    dp++; // Move past sample type.

    float tas = Tap2DToTAS((Tap2D *)dp++);
    if (tas < 0.0 || tas > 300.0) {
        std::stringstream msg("out of range, ");
        msg << tas;
        throw n_u::InvalidParameterException(getName(), "TAS", msg.str());
    }

    float frequency = getResolutionMicron() / tas;

    // Byte-swap the whole record up front.
    uint32_t  * p = (uint32_t *)dp;
    for (size_t i = 0; i < 1024; ++i, ++p)
        *p = bigEndian->int32Value(p);

    /* Loop through all slices in record.  Start at slice 1 since Spowart
     * decided to overwrite the first slice with the overload word...
     */
    p = (uint32_t *)dp;
    unsigned int overld = 0;
    if ((*p & 0xffff0000) == 0x55aa0000)
    {
      overld = (*p & 0x0000ffff) / 2000;
      _overLoadSliceCount++;
    }

    ++p;

    unsigned long long tBarElapsedtime = 0;  // Running accumulation of time-bars
    for (size_t i = 1; i < 1024; ++i, ++p)
    {

        /* Four cases; syncWord, timeWord, blank slice and legitimate slice.  syncWord
         * starts a particle.  Blank terminates a particle.
         */

        /* Overload occurs at record mid-point.  Add it in.
         */
        if (i == 512 && overld > 0)
            tBarElapsedtime += overld;
        else
        if (*p == _syncWord) {
            _totalParticles++;
            if (_cp)
                delete _cp;
            _cp = new Particle;
            _cp->width = 1;  // First slice generally considered lost.
            }
        else
        if (*(unsigned char *)p == _syncChar) {
	    unsigned int timeWord = 
	      (unsigned int)((p[1] & 0x00ffffff) * frequency);
            tBarElapsedtime += timeWord;
            unsigned long long thisParticleSecond = startTime + tBarElapsedtime;
            thisParticleSecond -= (thisParticleSecond % USECS_PER_SEC);

            // If we have crossed the 1 second boundary, send existing data and reset.
            if (thisParticleSecond != _nowTime)
            {
                sendData(_nowTime, results);
                _nowTime = thisParticleSecond;
                rc = true;
            }
        }
        else
        // Blank slice.
        if (*p == 0xffffffffL) {
            if (_cp) {
                countParticle(_cp, frequency);
                delete _cp; _cp = 0;
            }
        }
        else {
            processParticleSlice(_cp, (const unsigned char *)p);
        }
    }


    unsigned long long nt;
    nt = samp->getTimeTag();
    nt -= (nt % USECS_PER_SEC); // to seconds

    // If we have crossed the 1 second boundary, send existing data and reset.
    if (nt != _nowTime) {
        sendData(_nowTime, results);
        rc = true;
    }

    /* Force _nowTime to the TimeTag for this record, which will be the start time
     * for the next record.
     */
    _nowTime = nt;
    return rc;
}

bool TwoD32_USB::process(const Sample * samp,
                        list < const Sample * >&results) throw()
{
    if (samp->getDataByteLength() < sizeof (int32_t))
        return false;

    const int32_t *lptr =
        (const int32_t *) samp->getConstVoidDataPtr();
    int stype = bigEndian->int32Value(lptr++);

    /* From the usbtwod driver: stype=0 is image data, stype=1 is not used in 32 probe.  */
    if (stype == 0) {
        return processImage(samp, results);
    }
    return false;
}
