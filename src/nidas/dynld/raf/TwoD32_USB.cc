// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/


#include <nidas/linux/usbtwod/usbtwod.h>
#include <nidas/dynld/raf/TwoD32_USB.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, TwoD32_USB)

const unsigned char TwoD32_USB::_overldString[] = {0x55, 0xaa};
const unsigned char TwoD32_USB::_blankString[] = {0xff, 0xff, 0xff, 0xff};


TwoD32_USB::TwoD32_USB()
{
}

TwoD32_USB::~TwoD32_USB()
{
}

bool TwoD32_USB::processImage(const Sample * samp,
                             list < const Sample * >&results, int stype) throw()
{
    unsigned int slen = samp->getDataByteLength();
    const int wordSize = 4;
    const int tap2dSize = sizeof(Tap2Dv1);

    assert(tap2dSize == 4);

    if (slen < sizeof (int32_t) + tap2dSize) return false;
    _totalRecords++;
    _recordsPerSecond++;

    dsm_time_t startTime = _prevTime;
    _prevTime = samp->getTimeTag();

    if (startTime == 0) return false;

    const unsigned char * cp = (const unsigned char*) samp->getConstVoidDataPtr();
    const unsigned char * eod = cp + slen;
    cp += sizeof(int32_t); // Move past sample type.

    float tas = 0.0;
    if (stype == TWOD_IMGv2_TYPE) {
        Tap2D tap;
        memcpy(&tap,cp,sizeof(tap));
        cp += sizeof(Tap2D);
        tap.ntap = littleEndian->uint16Value(tap.ntap);
        tas = Tap2DToTAS(&tap);
    }
    else
    if (stype == TWOD_IMG_TYPE) {
        tas = Tap2DToTAS((Tap2Dv1 *)cp);
        cp += sizeof(Tap2Dv1);
    }
    else
        WLOG(("%s: Invalid IMG type, setting true airspeed to 0.",getName().c_str()));

    if (tas < 0.0 || tas > 300.0) {
        WLOG(("%s: TAS=%.1f is out of range",getName().c_str(),tas));
        _tasOutOfRange++;
        return false;
    }

    setupBuffer(&cp, &eod);
    const unsigned char * sod = cp;

    float resolutionUsec = getResolutionMicron() / tas;

    unsigned int overld = 0;
    unsigned int tBarElapsedTime = 0;  // Running accumulation of time-bars

    // Loop through all slices in record. We look in every byte for the
    // start of a sync word, so we can't scan the last 4 bytes, but save
    // them for the next processImage().
    for (; cp < eod - wordSize; )
    {
        /* Four cases, syncWord, overloadWord, blank or legitimate slice.
         * sync & overload words come at the end of the particle.  In the
         * case of the this probe, the time word is embedded in the sync
         * and overload word.
         */

        /* Scan next 4 bytes starting at current pointer, cp, for
         * a possible syncWord or overloadWord */
        const unsigned char* eow = cp + wordSize;

        const unsigned char* sos = cp;       // possible start of particle slice

        for (; cp < eow; ) {
            switch (*cp) {
            case 0x55:  // overload (0x55aa) or sync (0x55*) string
                if ((unsigned long)cp % wordSize) _misAligned++;
                if (::memcmp(cp, _overldString, sizeof(_overldString)) == 0) {
                    // overload word, reject particle.
                    _overLoadSliceCount++;
                    overld = (bigEndian->int32Value(cp) & 0x0000ffff) / 2000;
#ifdef DEBUG
                    cerr << "overload value at word " << (cp - sod)/wordSize << 
                        " is " << overld << endl;
#endif
                    tBarElapsedTime += overld;
                    if (cp - sod != (eod - sod)/2) _particle.zero();
                }
                else {
                    unsigned int timeWord = bigEndian->int32Value(cp) & 0x00ffffff;
                    if (timeWord == 0) {    // start of particle
                        _totalParticles++;
                        _particle.zero();
                        _particle.width = 1;  // First slice generally considered lost.
                    }
                    else
                    /* This is to catch suspect sync words observed in PLOWS.  There may be a
                     * better or different approach that we want to take here....  CJW/ 12-03-09
                     */
                    if ((timeWord & 0x00c00000) != 0) {
                        cerr << "PMS2D" << getSuffix() << " suspect timing/sync word, 0x55"
                             << hex << timeWord << dec;
                        cerr << ", " << n_u::UTime(samp->getTimeTag()).format(true,"%y/%m/%d %H:%M:%S.%6f")
                             << endl;
                        _totalParticles++;
                        _particle.zero();
                        _particle.width = 1;  // First slice generally considered lost.
                    }
                    else {
                        timeWord = (unsigned int)(timeWord * resolutionUsec);   // end of particle
                        tBarElapsedTime += timeWord;
                        dsm_time_t thisParticleTime = startTime + tBarElapsedTime;
                        if (thisParticleTime <= samp->getTimeTag()+3000000)
                            createSamples(thisParticleTime,results);
                        else { cerr << "PMS2D" << getSuffix() <<
                            " thisParticleTime in the future, not calling createSamples(), " << tBarElapsedTime << endl;
                            cerr << "  " << n_u::UTime(samp->getTimeTag()).format(true,"%y/%m/%d %H:%M:%S.%6f") <<
				", " << n_u::UTime(startTime).format(true,"%y/%m/%d %H:%M:%S.%6f") <<
				", " << n_u::UTime(thisParticleTime).format(true,"%y/%m/%d %H:%M:%S.%6f") << endl;
                        }
                    }
                }
                cp += wordSize;
                sos = 0;    // not a particle slice
                break;
            default:
                cp++;
                break;
            }
        }
        if (sos) {
            if (::memcmp(sos,_blankString,sizeof(_blankString)) == 0) {
                countParticle(_particle, resolutionUsec);
                _particle.zero();
            }
            else processParticleSlice(_particle, sos);
            cp = sos + wordSize;
        }
    }

    createSamples(samp->getTimeTag(),results);

    /* Data left in image block, save it in order to pre-pend to next image block */
    saveBuffer(cp,eod);

    return !results.empty();
}

bool TwoD32_USB::process(const Sample * samp,
                        list < const Sample * >&results) throw()
{
    if (samp->getDataByteLength() < sizeof (int32_t))
        return false;

    int stype = bigEndian->int32Value(samp->getConstVoidDataPtr());

    /* From the usbtwod driver: stype=0 is image data, stype=1 is not used in 32 probe.  */
    if (stype != TWOD_SOR_TYPE) {
        return processImage(samp, results, stype);
    }
    return false;
}
