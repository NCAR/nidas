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

#include "TwoD_SPEC.h"

#include <nidas/core/Parameter.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iostream>
#include <sstream>
#include <iomanip>

#include "SPEC-Probe/SPEC-RLE.hh"


using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

const unsigned char TwoD_SPEC::_syncString[] = { 0xaa, 0xaa, 0xaa, 0xaa };
const unsigned char TwoD_SPEC::_blankString[] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


TwoD_SPEC::TwoD_SPEC(std::string name)
    : _name(name), _processor(0), _spec(0),
      _compressedParticle(0), _uncompressedParticle(0),
      _prevParticleID(0), _timingWordMask(0x00000000ffffffffULL),
      _freq(0), _timingWordSize(2), _timingWordMSWFirst(true)
{

}

TwoD_SPEC::~TwoD_SPEC()
{
    delete [] _uncompressedParticle;
    delete [] _compressedParticle;
    delete _spec;
    delete _processor;
}

void TwoD_SPEC::init()
{
    UDPSocketSensor::init();

    // Post processing only.

    _processor = new TwoD_Processing(_name, NumberOfDiodes(), this);
    _processor->init();

    float tas = 200.0;  // Hack for now, since we have no access to a TAS.
    _freq = (double)getResolutionMicron() / tas;    // In microseconds.

    _spec = new SpecDecompress(_timingWordSize, false);
    _compressedParticle = new uint16_t[1024];

    // 1024 slices times 16 bytes per slice.
    _uncompressedParticle = new uint8_t[1024*16];
}


/*---------------------------------------------------------------------------*/
bool TwoD_SPEC::process(const Sample * samp, list < const Sample * >&results)
{
    const char *input = (const char*) samp->getConstVoidDataPtr();
    bool result = false;

    unsigned nbytes = samp->getDataByteLength();

//    const unsigned char* ip = input;
//    const unsigned char* eoi = input + nbytes;

    DLOG( ("") << _name << ": nBytes = " << nbytes );


    if (!strncmp(input, "SPEC2D,", 7) || !strncmp(input, "SPECHVPS,", 9))
        result = processHousekeeping(samp, results);    // len == ~250
    else
    {
        _processor->_totalRecords++;
        _processor->_recordsPerSecond++;
        result = processImageRecord(samp, results); // len == 4111
    }

    return result;
}

/*---------------------------------------------------------------------------*/
bool TwoD_SPEC::processImageRecord(const Sample * samp, list < const Sample * >&results)
{
    const unsigned char *cp = (const unsigned char *)samp->getConstVoidDataPtr();

    // slen is coming in as 4098 bytes for image buffer, no timestamp or cksum.
//    unsigned slen = samp->getDataByteLength();
//cout << _name << "::processImage, slen=" << slen << "\n";


    // Use DSM time tags, since we don't have probe timestamps.
    dsm_time_t startTime = _processor->_prevTime;
    _processor->_prevTime = samp->getTimeTag();

    if (startTime == 0) return false;

//    const unsigned char * eod = cp + slen;
    dsm_time_t firstTimeWord = 0;

    // Restore any saved buffer from previous call.
// Since we don't have all the data from the probe, we can't wrap around
// buffers, treat each buffer as standalone
//    _processor->setupBuffer(&cp, &eod);


    uint16_t *wp = (uint16_t *)cp;
    int nImgWords;

    for (size_t j = 0; j < 2043; ++j)   // want at least 5 words, otherwise save
    {
        cp = (uint8_t *)&wp[j];

        if (wp[j] == _spec->FlushWord)          // NL flush buffer
        {
            // we want to make sure the buffer is discarded, there is no more data
            DLOG( ("") << _name << ": NL flush @ idx = " << j );
//            cout << "NL flush @ idx = " << j << "\n";
            _processor->createSamples(samp->getTimeTag(), results);

//            eod = cp;
//            _processor->saveBuffer(cp, eod);
            return !results.empty();
        }


        // Start of particle
        if ( _spec->isParticleSyncWord(&wp[j]) )
        {
            bool reject = false;

//            cout << " start of particle, j=" << j << " NH/NV=" << wp[j+1] << ", " << wp[j+2] << endl;

if ((wp[j+1] & 0x0FFF) == 0) continue;  // Horizontal only at this time.

            // Do not process multi-packet images.
            if (wp[j+1] & 0x1000 || wp[j+2] & 0x1000) break;

            memcpy(_compressedParticle, &wp[j], 5 * sizeof(uint16_t));
            nImgWords = _spec->extractNimageWords(_compressedParticle); // get number of image words to copy

            if (nImgWords == 0 || nImgWords > 950) // seems runaway @ 960ish
                reject = true;

            // Skip particles where FIFO goes into overload
            if (wp[j+1] & 0x8000 || wp[j+2] & 0x8000) reject = true;

            // I am choosing not to deal with multi-packet particles.  If you do, then
            // make sure to understand that only the last packet has a timing word.
            if (_compressedParticle[_spec->ID] == _prevParticleID || _spec->_multiPacketParticle)
                reject = true;

            j += 5;
            _prevParticleID = _compressedParticle[_spec->ID];

            if (reject)
            {
                j += nImgWords -1;
                continue;
            }

            if (j + nImgWords > 2048)   // Crosses into next buffer
            {
                // Save off and leave.
                DLOG( ("") << _name << ": short image, j=" << j << ", n=" << nImgWords);
                break;
            }

            memcpy(&_compressedParticle[5], &wp[j], nImgWords * sizeof(uint16_t));
            j += (nImgWords-1);

            // Decompress particle and get stats.
            _processor->_particle.zero();
            _processor->_totalParticles++;
            size_t nSlices = _spec->decompressParticle(_compressedParticle, _uncompressedParticle);

            // Error in particle
            if (nSlices == 0)
                continue;
if (nSlices > 575) WLOG( ("") <<_name << " - nSlices = " << nSlices << "  !!!");
            // if no sync/timing word, then we are dropping multi-packet image.
            // This is where to fix it, buffer them up
            if (memcmp((void *)&_uncompressedParticle[nSlices*16+8], &_syncString, 4))
                continue;

            // nSlices-1, since timing word is being counted.
            for (size_t k = 0; k < nSlices-1; ++k)
            {
                _processor->processParticleSlice(&_uncompressedParticle[k*16]);
            }

            // Get time.  Type32 stores the timing word most-significant-word-first;
            // Type48 stores it least-significant-word-first.
            dsm_time_t thisTimeWord = 0;
            const uint16_t *tw = &_compressedParticle[5 + nImgWords - _timingWordSize];
            for (int w = 0; w < _timingWordSize; ++w)
            {
                int shift = _timingWordMSWFirst ? (_timingWordSize - 1 - w) : w;
                thisTimeWord |= (dsm_time_t)tw[w] << (16 * shift);
            }
            thisTimeWord &= _timingWordMask;

            if (firstTimeWord == 0)
                firstTimeWord = thisTimeWord;

            // Record time tag minus approx microseconds since start of record.
            // The probe's timing word counter can roll over in the middle of
            // an image record (e.g. every ~54 minutes for Type32), so correct
            // for a single wrap of the counter.  A genuine rollover always
            // yields a small corrected offset, bounded by how much real time
            // one image record can span.  Type48's counter is wide enough that
            // it essentially never rolls over within a flight, so a backward
            // jump there is almost always a corrupt timing word, not a wrap;
            // guard against "correcting" that into a bogus far-future time.
            static const double maxPlausibleRecordUsec = 120.0e6;   // 120 sec
            double diffTimeLine = 0.0;

            if (firstTimeWord <= thisTimeWord)
            {
              diffTimeLine = (double)(thisTimeWord - firstTimeWord) * _freq;
            }
            else
            {
              dsm_time_t counterRange = (dsm_time_t)_timingWordMask + 1;
              double wrapped = (double)(thisTimeWord + counterRange - firstTimeWord) * _freq;

              if (wrapped <= maxPlausibleRecordUsec)
              {
                diffTimeLine = wrapped;
                ILOG( ("") << _name << ": timing word rollover in image record, corrected" );
              }
              else
              {
                ELOG( ("") << _name << ": thisTimeWord < firstTime; implausible rollover, ignoring offset" );
                ELOG( ("") << _name << ":   thisTimeLine = " << thisTimeWord << ", firstTimeWord = " << firstTimeWord );
              }
            }
/*
cout << "firstTimeLine = " << firstTimeWord << ", thisTimeWord = " << thisTimeWord << "\n";
cout << "  diff = " << thisTimeWord - firstTimeWord << " - " << diffTimeLine << "\n";
cout << "  diff TimeLine = " << diffTimeLine << " - " << (dsm_time_t)diffTimeLine << "\n";
*/
            dsm_time_t thisParticleTime = startTime + (dsm_time_t)diffTimeLine;

            _processor->countParticle(0);
            _processor->createSamples(thisParticleTime, results);
        }
    }

    _processor->createSamples(samp->getTimeTag(), results);

    /* Data left in image block, save it in order to pre-pend to next image block */
//    _processor->saveBuffer(cp, eod);

    return !results.empty();
}

/*---------------------------------------------------------------------------*/
bool TwoD_SPEC::processHousekeeping(const Sample * samp, list < const Sample * >&results)
{
    return CharacterSensor::process(samp, results);
}

/*---------------------------------------------------------------------------*/
void TwoD_SPEC::printStatus(std::ostream& ostr)
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td></tr>" << endl;
	return;
    }
/*
    struct usb_twod_stats status;

    try {
	ioctl(USB2D_GET_STATUS,&status,sizeof(status));
	long long tnow = n_u::getSystemTime();
	float imagePerSec = float(status.numImages - _numImages) /
		float(tnow - _lastStatusTime) * USECS_PER_SEC;
	_numImages = status.numImages;
	_lastStatusTime = tnow;

	ostr << "<td align=left>" << "imgBlks/sec=" <<
		fixed << setprecision(1) << imagePerSec <<
		",lost=" << status.lostImages << ",lostSOR=" << status.lostSORs <<
		",lostTAS=" << status.lostTASs << ", urbErrs=" << status.urbErrors <<
                ",TAS=" << setprecision(0) << _trueAirSpeed << "m/s" <<
		"</td></tr>" << endl;
    }
    catch(const n_u::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td></tr>" << endl;
	n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: printStatus: %s",getName().c_str(),
            ioe.what());
    }
*/
}
