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

// Verbose log messages are now compiled by default, but they can only be
// enabled by explicitly enabling verbose logging in a LogConfig, such as
// with a command-line option.
#define SLICE_DEBUG

#include <nidas/linux/usbtwod/usbtwod.h>
#include "TwoD64_USB.h"
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/Variable.h>

#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;
using nidas::util::endlog;

NIDAS_CREATOR_FUNCTION_NS(raf, TwoD64_USB)

const unsigned char TwoD64_USB::_syncString[] = { 0xaa, 0xaa, 0xaa };
const unsigned char TwoD64_USB::_overldString[] = { 0x55, 0x55, 0xaa };
const unsigned char TwoD64_USB::_blankString[] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


TwoD64_USB::TwoD64_USB(): _blankLine(false),
     _prevTimeWord(0),                          
     _probeClockRate(12),                        //Default for v2 is 12 MHZ
     _timeWordMask(0x000000ffffffffffLL)         //Default for v2 is 40 bits
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
    list<SampleTag *>& tags = getSampleTags();
    list<SampleTag *>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag * tag = *si;
        Variable & var = ((SampleTag *)tag)->getVariable(0);

        if (var.getName().compare(0, 5, "SHDOR") == 0) {
            sorRate = tag->getRate();
            _sorID = tag->getId();
        }
    }
    if (sorRate <= 0.0) throw n_u::InvalidParameterException(getName(),
	"sample","shadow OR sample rate not found");
    if (sorRate != getTASRate()) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
		"%s: shadowOR sample rate=%f is not equal to TAS_RATE=%f, continuing",
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

struct PTime
{
    PTime(dsm_time_t tt, bool showdate=true) :
        _tt(tt),
        _showdate(showdate)
    {}

    dsm_time_t _tt;
    bool _showdate;
};

inline
std::ostream&
operator<<(std::ostream& out, const PTime& ptime)
{
    if (ptime._showdate)
        out << n_u::UTime(ptime._tt).format(true, "%y/%m/%d %H:%M:%S.%6f");
    else
        out << n_u::UTime(ptime._tt).format(true, "%H:%M:%S.%6f");
    return out;
}


bool TwoD64_USB::processImageRecord(const Sample * samp,
                             list < const Sample * >&results, int stype) throw()
{
    unsigned int slen = samp->getDataByteLength();
    const int wordSize = 8;

    assert(sizeof(Tap2D) == 4);
    if (slen < sizeof (int32_t) + sizeof(Tap2D)) return false;
    _totalRecords++;
    _recordsPerSecond++;

#ifdef SLICE_DEBUG
    unsigned int sampTdiff = samp->getTimeTag() - _prevTime;
#endif

    dsm_time_t startTime = _prevTime;
    _prevTime = samp->getTimeTag();

    if (startTime == 0) return false;

    const unsigned char * cp = (const unsigned char *) samp->getConstVoidDataPtr();
    const unsigned char * eod = cp + slen;

    cp += sizeof(int32_t); // Move past sample type.

    /// @todo don't do this in real-time?
    //scanForMissalignedSyncWords(samp, (unsigned char *)dp);

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

    // Lifepsan of a single slice.
    float resolutionUsec = getResolutionMicron() / tas;

    setupBuffer(&cp,&eod);

#ifdef SLICE_DEBUG
    // Create a log point here for verbose slice logging.  If active, then
    // the message accumulates while iterating through slices, and then it
    // is logged.  There should be minimal overhead as long as nothing is
    // streamed when this log context is not enabled.
    static n_u::LogContext sdlog(LOG_VERBOSE, "slice_debug");
    static n_u::LogMessage sdmsg(&sdlog);
    const unsigned char* sod = cp;
    if (sdlog.active())
    {
        sdmsg << endlog
              << PTime(samp->getTimeTag())
              << ", start@" << PTime(startTime, false)
              << " image=" << (slen - 8) << " + " <<  ((eod-cp) - (slen - 8))
              << " bytes -----" << endlog;
    }
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
        if (sdlog.active())
        {
            // Put only so many of the slice dumps on a line.
            if (sdmsg.length() > 80)
            {
                sdmsg.log();
            }
            sdmsg << dec << _totalRecords << ' ' << setw(3)
                  << (unsigned long)(cp - sod)/wordSize << ' '
                  << (unsigned long)cp % 8 << ' ';
            sdmsg << hex;
        }
        // bool suspect = false;
#endif

        /* Scan next 8 bytes starting at current pointer, cp, for
         * a possible syncWord or overloadWord */
        const unsigned char* eow = cp + wordSize;

        for (; cp < eow; ) {
#ifdef SLICE_DEBUG
            if (sdlog.active())
            {
                sdmsg << setw(2) << (int)*cp << ' ';
            }
#endif
            switch (*cp) {
            case 0x55:  // start of possible overload string
                if (cp + wordSize > eod) {
                    createSamples(samp->getTimeTag(), results);
                    saveBuffer(cp,eod);
                    return !results.empty();
                }
                if (::memcmp(cp+1,_overldString+1,sizeof(_overldString)-1) == 0) {
                    // match to overload string

                    // time words are from a 12MHz clock
                    long long thisTimeWord =
                        (bigEndian->int64Value(cp) & _timeWordMask ) / _probeClockRate;

                    if (firstTimeWord == 0)
                        firstTimeWord = thisTimeWord;

                    WLOG(("Fast2D") << getSuffix() << " overload at : "
                         << PTime(samp->getTimeTag())
                         << ", duration "
                         << (thisTimeWord - _prevTimeWord) / 1000);

#ifdef SLICE_DEBUG
                    if (sdlog.active())
                    {
                        for (const unsigned char* xp = cp; ++xp < cp + wordSize; )
                            sdmsg << setw(2) << (int)*xp << ' ';
                    }
#endif
                    // overload word, reject particle.
                    _overLoadSliceCount++;
                    if (((unsigned long)cp - _savedBytes) % wordSize)
                    {
                        _misAligned++;
#ifdef SLICE_DEBUG
                        if (sdlog.active())
                        {
                            sdmsg << dec << " misaligned ovld" << endlog;
                        }
#endif
                    }
#ifdef SLICE_DEBUG
                    else if (sdlog.active())
                    {
                        sdmsg << dec << " ovld" << endlog;
                    }
#endif
                    _particle.zero();
                    _blankLine = false;
                    cp += wordSize;
                    sos = 0;    // not a particle slice

                    long long thisParticleTime = startTime + (thisTimeWord - firstTimeWord);
                    long usec = thisParticleTime % USECS_PER_SEC;
                    long dt = (thisTimeWord - _prevTimeWord);	// actual overload/dead time.

                    /* dt can go negative if the probe has been reset and
                     * the internal clock starts at zero again.  Ignore if
                     * that is the case since we have no meaningful delta
                     * time.
                     */
                    if (dt > 0) {	// If probe was not reset.
                        if (dt < usec)
                            // Dead time falls in normal boundaries, add it in.
                            _dead_time += dt;
                        else
                            /* Dead time is large, perhaps more than a
                             * second, but don't add all that into this
                             * second, dead time can not exceed 1 second.
                             * Make dead from the beginning of the second
                             * to the actual time stamp of the overload
                             * time slice.
                             */
                            _dead_time += usec;
                    }
                    _prevTimeWord = thisTimeWord;
                }
                else if (*(cp+1) == (unsigned char)'\x55') {
                    // 0x5555 but not complete overload string
#ifdef SLICE_DEBUG
                    if (sdlog.active())
                    {
                        for (const unsigned char* xp = cp; ++xp < cp + wordSize; )
                        {
                            sdmsg << setw(2) << (int)*xp << ' ';
                        }
                        sdmsg << dec << " 5555 ovld" << endlog;
                    }
#endif
                    cp += wordSize;     // skip
                    sos = 0;    // not a particle slice
                }
                else {
                    // 0x55 is not an expected particle shadow.
                    _suspectSlices++;
                    // suspect = true;
                    cp++;
                }
                break;
            case 0xaa:  // possible syncword and time. Terminates particle
                if (cp + wordSize > eod) {
                    createSamples(samp->getTimeTag(), results);
                    saveBuffer(cp,eod);
                    return !results.empty();
                }
                if (::memcmp(cp+1,_syncString+1,sizeof(_syncString)-1) == 0) {
                    // syncword
                    _totalParticles++;
#ifdef SLICE_DEBUG
                    if (sdlog.active())
                    {
                        for (const unsigned char* xp = cp; ++xp < cp + wordSize; )
                            sdmsg << setw(2) << (int)*xp << ' ';
                    }
#endif
                    if (((unsigned long)cp - _savedBytes) % wordSize) {
                        _misAligned++;
#ifdef SLICE_DEBUG
                        if (sdlog.active())
                        {
                            sdmsg << dec << " misaligned sync" << endlog;
                            sdmsg << "Misaligned data at "
                                  << PTime(samp->getTimeTag())
                                  << endlog;
                        }
#endif
                    }
#ifdef SLICE_DEBUG
                    else if (sdlog.active())
                    {
                        sdmsg << dec << " sync" << endlog;
                    }
#endif
                    // time words are from a 12MHz clock
                    long long thisTimeWord =
                        (bigEndian->int64Value(cp) & _timeWordMask) /_probeClockRate;

                    if (firstTimeWord == 0)
                        firstTimeWord = thisTimeWord;

                    // Approx microseconds since start of record.
                    long long thisParticleTime = startTime + (thisTimeWord - firstTimeWord);

#ifdef SLICE_DEBUG
                    if (sdlog.active())
                    {
                        sdmsg
                            << PTime(thisParticleTime)
                            << " p.edgeTouch=" << hex << setw(2)
                            << (int)_particle.edgeTouch
                            << dec
                            << " height=" << setw(2) << _particle.height
                            << " width=" << setw(4) << _particle.width
                            << " syncTdiff=" << (thisTimeWord - firstTimeWord)
                            << " sampTdiff=" << sampTdiff << endlog;
                    }
#endif
                    // If we have crossed the end of the histogram period,
                    // send existing data and reset.  Don't create samples
                    // too far in the future, say 5 seconds.  @TODO look
                    // into what is wrong, or why this offset is needed.
                    if (thisParticleTime <= samp->getTimeTag()+5000000)
                        createSamples(thisParticleTime, results);
                    else
                    {
                        WLOG(("Fast2DC")
                             << getSuffix()
                             <<
                             " thisParticleTime in the future, "
                             "not calling createSamples()");
                        WLOG(("  ")
                             << PTime(samp->getTimeTag()) << " "
                             << PTime(thisParticleTime));
                    }

                    // If there are any extra bytes between last particle slice
                    // and syncword then ignore last particle.
                    if (cp == sos)
                    {
                        countParticle(_particle, resolutionUsec);
                    }
#ifdef SLICE_DEBUG
                    else if (sdlog.active())
                    {
                        sdmsg << "discarded particle" << endlog;
                    }
#endif
                    _particle.zero();
                    _blankLine = false;
                    cp += wordSize;
                    sos = 0;    // not a particle slice
                    _prevTimeWord = thisTimeWord;
                }
                else if (*(cp+1) == (unsigned char)'\xaa') {
                    // 0xaaaa but not complete syncword
#ifdef SLICE_DEBUG
                    if (sdlog.active())
                    {
                        for (const unsigned char* xp = cp; ++xp < cp + wordSize; )
                        {
                            sdmsg << setw(2) << (int)*xp << ' ';
                        }
                        sdmsg << dec << " aaaa word" << endlog;
                    }
#endif
                    cp += wordSize;
                    sos = 0;    // not a particle slice
                }
                else {
                    // 0xaa is not an expected particle shadow.
                    sos = 0;    // not a particle slice
                    _suspectSlices++;
#ifdef SLICE_DEBUG
                    // suspect = true;
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

    return !results.empty();
}

bool TwoD64_USB::process(const Sample * samp,
                        list < const Sample * >&results) throw()
{
    assert(sizeof (long long) == 8);

    if (samp->getDataByteLength() < sizeof (int32_t))
        return false;

    int stype = bigEndian->int32Value(samp->getConstVoidDataPtr());

    /* From the usbtwod driver: stype=0 is image data, stype=1 is SOR.  */
    bool result = false;
    switch (stype) {
        case TWOD_IMG_TYPE:
        case TWOD_IMGv2_TYPE:
            result = processImageRecord(samp, results, stype);
        case TWOD_SOR_TYPE:	// Shadow-or counter.
            result = processSOR(samp, results);
    }

    static n_u::LogContext sdlog(LOG_VERBOSE, "slice_debug");
    if (sdlog.active())
    {
        static n_u::LogMessage sdmsg(&sdlog);

        for (list<const Sample*>::iterator it = results.begin();
             it != results.end(); ++it)
        {
            const SampleT<float>* outs =
                dynamic_cast<const SampleT<float>*>(*it);
            dsm_time_t timetag = outs->getTimeTag();
            dsm_sample_id_t sid = outs->getId();
            const float* dout = outs->getConstDataPtr();
            const float* dend = dout + outs->getDataLength();
            if (sid == _1dcID)
            {
                sdmsg << "1D sample@" << PTime(timetag) << ": ";
                unsigned int nsizes = NumberOfDiodes();
                stream_histogram(sdmsg, dout, nsizes);
                dout += nsizes;
                if (dout < dend)
                    sdmsg << ", dt=" << *dout++;
                if (dout < dend)
                    sdmsg << ", rps=" << *dout++;
                sdmsg << endlog;
            }
            else if (sid == _2dcID)
            {
                sdmsg << "1D sample@" << PTime(timetag) << ": ";
                unsigned int nsizes = NumberOfDiodes() << 1;
                stream_histogram(sdmsg, dout, nsizes);
                dout += nsizes;
                if (dout < dend)
                    sdmsg << ", dt=" << *dout++;
                sdmsg << endlog;
            }
            else if (sid == _sorID)
            {
                sdmsg << "SOR sample@" << PTime(timetag) << ": "
                      << *dout << endlog;
            }
        }
    }
    return result;
}
