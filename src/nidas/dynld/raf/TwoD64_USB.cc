/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3716 $

    $LastChangedDate: 2007-03-08 13:43:19 -0700 (Thu, 08 Mar 2007) $

    $LastChangedRevision: 3716 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/TwoD64_USB.cc $

 ******************************************************************
*/

// #define SLICE_DEBUG

#include <nidas/linux/usbtwod/usbtwod.h>
#include <nidas/dynld/raf/TwoD64_USB.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, TwoD64_USB)

const unsigned char TwoD64_USB::_syncString[] = { 0xaa, 0xaa, 0xaa };
const unsigned char TwoD64_USB::_overldString[] = { 0x55, 0x55, 0xaa };
const unsigned char TwoD64_USB::_blankString[] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


TwoD64_USB::TwoD64_USB(): _blankLine(false)
{
}

TwoD64_USB::~TwoD64_USB()
{
}

void TwoD64_USB::init_parameters()
    throw(n_u::InvalidParameterException)
{
    TwoD_USB::init_parameters();

    /* Look for a sample tag with id=2. This is assumed to be
     * the shadowOR sample.  Check its rate.
     */
    float sorRate = 0.0;
    list<const SampleTag *> tags = getSampleTags();
    list<const SampleTag *>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag * tag = *si;
        Variable & var = ((SampleTag *)tag)->getVariable(0);

        if (var.getName().compare(0, 6, "SHDORC") == 0) {
            sorRate = tag->getRate();
            _sorID = tag->getId();
        }
    }
    if (sorRate <= 0.0) throw n_u::InvalidParameterException(getName(),
	"sample","shadow OR sample rate not found");
    if (sorRate != getTASRate()) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s: shadowOR sample rate=%f is not equal to TAS_RATE=%f, continuing\n",
		getName().c_str(),sorRate,getTASRate());
    }
}

bool TwoD64_USB::processSOR(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    if (samp->getDataByteLength() < 2 * sizeof (int32_t))
        return false;

    const int32_t *lptr =
        (const int32_t *) samp->getConstVoidDataPtr();

    /*int stype =*/ bigEndian->int32Value(*lptr++);
    int sor = bigEndian->int32Value(*lptr++);

    size_t nvalues = 1;
    SampleT < float >*outs = getSample < float >(nvalues);
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(_sorID);
    float *dout = outs->getDataPtr();
    *dout = sor;

    results.push_back(outs);
    return true;
}

bool TwoD64_USB::processImageRecord(const Sample * samp,
                             list < const Sample * >&results, int stype) throw()
{
    unsigned int slen = samp->getDataByteLength();
    const int wordSize = 8;

    static long long prevTimeWord = 0;

    assert(sizeof(Tap2D) == 4);
    if (slen < sizeof (int32_t) + sizeof(Tap2D)) return false;
    _totalRecords++;

#ifdef SLICE_DEBUG
    unsigned int sampTdiff = samp->getTimeTag() - _prevTime;
#endif

    long long startTime = _prevTime;
    _prevTime = samp->getTimeTag();

    if (startTime == 0) return false;

    const unsigned char * cp = (const unsigned char *) samp->getConstVoidDataPtr();
    const unsigned char * eod = cp + slen;

    cp += sizeof(int32_t); // Move past sample type.

    /// @todo don't do this in real-time?
    //scanForMissalignedSyncWords(samp, (unsigned char *)dp);

    float tas = 0.0;
    if (stype == TWOD_IMGv2_TYPE) {
        tas = Tap2DToTAS((Tap2D *)cp);
        cp += sizeof(Tap2D);
    }
    else
    if (stype == TWOD_IMG_TYPE) {
        tas = Tap2DToTAS((Tap2Dv1 *)cp);
        cp += sizeof(Tap2Dv1);
    }
    else
        WLOG(("%s: Invalid IMG type, setting true airspeed to 0.\n",getName().c_str()));

    if (tas < 0.0 || tas > 300.0) {
        WLOG(("%s: TAS=%.1f is out of range\n",getName().c_str(),tas));
        _tasOutOfRange++;
        return false;
    }

    // Lifepsan of a single slice.
    float resolutionUsec = getResolutionMicron() / tas;

    setupBuffer(&cp,&eod);

#ifdef SLICE_DEBUG
    const unsigned char* sod = cp;
    cerr << endl << n_u::UTime(samp->getTimeTag()).format(true,"%y/%m/%d %H:%M:%S.%6f") <<
        " image=" << (slen - 8) << " + " <<  ((eod-cp) - (slen - 8)) << " bytes -----"<< endl;
#endif

    // Loop through all slices in record.
    long long firstTimeWord = 0;	// First timing word in this record.
    for (; cp < eod - (wordSize - 1); )
    {
        /* Four cases, syncWord, overloadWord, blank or legitimate slice.
         * sync & overload words come at the end of the particle.  In the
         * case of the this probe, the time word is embedded in the sync
         * and overload word.
         */

        // possible start of particle slice if it isn't a sync or overload word
        const unsigned char* sos = cp;

#ifdef SLICE_DEBUG
        cerr << dec << _totalRecords << ' ' << setw(3) <<
            (unsigned long)(cp - sod)/wordSize << ' ' << (unsigned long)cp % 8 << ' ';
        cerr << hex;
        bool suspect = false;
#endif

        /* Scan next 8 bytes starting at current pointer, cp, for
         * a possible syncWord or overloadWord */
        const unsigned char* eow = cp + wordSize;

        for (; cp < eow; ) {
#ifdef SLICE_DEBUG
            cerr << setw(2) << (int)*cp << ' ';
#endif
            switch (*cp) {
            case 0x55:  // start of possible overload string
                if (cp + wordSize > eod) {
                    createSamples(samp->getTimeTag(), results);
                    saveBuffer(cp,eod);
                    return results.size() > 0;
                }
                if (::memcmp(cp+1,_overldString+1,sizeof(_overldString)-1) == 0) {
                    // match to overload string

                    cerr << "Overload at : " << n_u::UTime(samp->getTimeTag()).format(true,"%H:%M:%S.%6f") << endl;

                    // time words are from a 12MHz clock
                    long long thisTimeWord =
                        (bigEndian->int64Value(cp) & 0x000000ffffffffffLL) / 12;

                    if (firstTimeWord == 0)
                        firstTimeWord = thisTimeWord;

                    _dead_time_1D += (thisTimeWord - prevTimeWord);
                    _dead_time_2D += (thisTimeWord - prevTimeWord);

#ifdef SLICE_DEBUG
                    for (const unsigned char* xp = cp; ++xp < cp + wordSize; )
                        cerr << setw(2) << (int)*xp << ' ';
#endif
                    // overload word, reject particle.
                    _overLoadSliceCount++;
                    if (((unsigned long)cp - _savedBytes) % wordSize) {
                        _misAligned++;
#ifdef SLICE_DEBUG
                        cerr << dec << " misaligned ovld" << endl;
#endif
                    }
#ifdef SLICE_DEBUG
                    else cerr << dec << " ovld" << endl;
#endif
                    _particle.zero();
                    _blankLine = false;
                    cp += wordSize;
                    sos = 0;    // not a particle slice
                    prevTimeWord = thisTimeWord;
                }
                else if (*(cp+1) == (unsigned char)'\x55') {
                    // 0x5555 but not complete overload string
#ifdef SLICE_DEBUG
                    for (const unsigned char* xp = cp; ++xp < cp + wordSize; )
                        cerr << setw(2) << (int)*xp << ' ';
                    cerr << dec << " 5555 ovld" << endl;
#endif
                    cp += wordSize;     // skip
                    sos = 0;    // not a particle slice
                }
                else {
                    // 0x55 is not an expected particle shadow.
                    _suspectSlices++;
#ifdef SLICE_DEBUG
                    suspect = true;
#endif
                    cp++;
                }
                break;
            case 0xaa:  // possible syncword and time. Terminates particle
                if (cp + wordSize > eod) {
                    createSamples(samp->getTimeTag(), results);
                    saveBuffer(cp,eod);
                    return results.size() > 0;
                }
                if (::memcmp(cp+1,_syncString+1,sizeof(_syncString)-1) == 0) {
                    // syncword
                    _totalParticles++;
#ifdef SLICE_DEBUG
                    for (const unsigned char* xp = cp; ++xp < cp + wordSize; )
                        cerr << setw(2) << (int)*xp << ' ';
#endif
                    if (((unsigned long)cp - _savedBytes) % wordSize) {
#ifndef SLICE_DEBUG
                          cerr << "Misaligned data at " << n_u::UTime(samp->getTimeTag()).format(true,"%H:%M:%S.%6f") << endl;
#endif
                        _misAligned++;
#ifdef SLICE_DEBUG
                        cerr << dec << " misaligned sync" << endl;
#endif
                    }
#ifdef SLICE_DEBUG
                    else cerr << dec << " sync" << endl;
#endif

                    // time words are from a 12MHz clock
                    long long thisTimeWord =
                        (bigEndian->int64Value(cp) & 0x000000ffffffffffLL) / 12;

                    if (firstTimeWord == 0)
                        firstTimeWord = thisTimeWord;

                    // Approx microseconds since start of record.
                    long long thisParticleTime = startTime + (thisTimeWord - firstTimeWord);

#ifdef SLICE_DEBUG
                    cerr << n_u::UTime(thisParticleTime).format(true,"%y/%m/%d %H:%M:%S.%6f") <<
                        " p.edgeTouch=" << hex << setw(2) << (int)_particle.edgeTouch << dec <<
                        " height=" << setw(2) << _particle.height <<
                        " width=" << setw(4) << _particle.width <<
                        " syncTdiff=" << (thisTimeWord - firstTimeWord) << 
                        " sampTdiff=" << sampTdiff << endl;
#endif
                    // If we have crossed the end of the histogram period, send existing
                    // data and reset.  Don't create samples too far in the future, say
                    // 5 seconds.  @TODO look into what is wrong, or why this offset is needed.
                    if (thisParticleTime <= samp->getTimeTag()+5000000)
                        createSamples(thisParticleTime, results);
//#ifdef SLICE_DEBUG
                    else { cerr << "thisParticleTime in the future, not calling createSamples()" << endl;
                        cerr << "  " << n_u::UTime(samp->getTimeTag()).format(true,"%y/%m/%d %H:%M:%S.%6f") <<
					n_u::UTime(thisParticleTime).format(true,"%y/%m/%d %H:%M:%S.%6f") << endl;
                    }
//#endif

                    // If there are any extra bytes between last particle slice
                    // and syncword then ignore last particle.
                    if (cp == sos) countParticle(_particle, resolutionUsec);
#ifdef SLICE_DEBUG
                    else cerr << "discarded particle" << endl;
#endif
                    _particle.zero();
                    _blankLine = false;
                    cp += wordSize;
                    sos = 0;    // not a particle slice
                    prevTimeWord = thisTimeWord;
                }
                else if (*(cp+1) == (unsigned char)'\xaa') {
                    // 0xaaaa but not complete syncword
#ifdef SLICE_DEBUG
                    for (const unsigned char* xp = cp; ++xp < cp + wordSize; )
                        cerr << setw(2) << (int)*xp << ' ';
                    cerr << dec << " aaaa word" << endl;
#endif
                    cp += wordSize;
                    sos = 0;    // not a particle slice
                }
                else {
                    // 0xaa is not an expected particle shadow.
                    sos = 0;    // not a particle slice
                    _suspectSlices++;
#ifdef SLICE_DEBUG
                    suspect = true;
#endif
                    cp++;
                }
                break;
            default:
                cp++;
                break;
            }
        }
        if (sos) {
            // scan 8 bytes of particle
            // If a blank string, then next word should be sync, otherwise discard it
            if (::memcmp(sos,_blankString,sizeof(_blankString)) != 0) {
                processParticleSlice(_particle, sos);
            }
            cp = sos + wordSize;
        }
    }

    createSamples(samp->getTimeTag(), results);

    /* Data left in image block, save it in order to pre-pend to next image block */
    saveBuffer(cp,eod);

    return results.size() > 0;
}

bool TwoD64_USB::process(const Sample * samp,
                        list < const Sample * >&results) throw()
{
    assert(sizeof (long long) == 8);

    if (samp->getDataByteLength() < sizeof (int32_t))
        return false;

    int stype = bigEndian->int32Value(samp->getConstVoidDataPtr());

    /* From the usbtwod driver: stype=0 is image data, stype=1 is SOR.  */
    switch (stype) {
        case TWOD_IMG_TYPE:
        case TWOD_IMGv2_TYPE:
            return processImageRecord(samp, results, stype);
        case TWOD_SOR_TYPE:	// Shadow-or counter.
            return processSOR(samp, results);
    }
    return false;
}
