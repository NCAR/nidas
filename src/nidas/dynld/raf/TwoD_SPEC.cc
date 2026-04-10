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

#include <nidas/dynld/raf/TwoD_Processing.h>

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

const unsigned char TwoD_SPEC::_syncString[] = { 0xaa, 0xaa };
const unsigned char TwoD_SPEC::_blankString[] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


TwoD_SPEC::TwoD_SPEC(std::string name)
    : _name(name), _processor(0), _spec(0),
      _compressedParticle(0), _uncompressedParticle(0),
      _prevParticleID(0), _timingWordMask(0x00000000ffffffffULL)
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
    // Post processing only.

    _processor = new TwoD_Processing(_name, NumberOfDiodes(), this);
    _processor->init();

    _spec = new SpecDecompress();
    _compressedParticle = new uint16_t[1024];
    _uncompressedParticle = new uint8_t[8192];
}


/*---------------------------------------------------------------------------*/
bool TwoD_SPEC::process(const Sample * samp, list < const Sample * >&results)
{
    const char *input = (const char*) samp->getConstVoidDataPtr();
    bool result = false;

    unsigned nbytes = samp->getDataByteLength();

//    const unsigned char* ip = input;
//    const unsigned char* eoi = input + nbytes;

    DLOG( ("raf.TwoDS: nBytes = ") << nbytes );


    if (!strncmp(input, "SPEC2D,", 6) || !strncmp(input, "SPECHVPS,", 8))
        result = processHousekeeping(samp, results);    // len == ~250
    else
        result = processImageRecord(samp, results); // len == 4121

    return result;
}

/*---------------------------------------------------------------------------*/
bool TwoD_SPEC::processImageRecord(const Sample * samp, list < const Sample * >&results)
{
    const unsigned char *cp = (unsigned char *)samp->getConstVoidDataPtr();

    // slen is coming in as 4098 bytes for image buffer, no timestamp or cksum.
    unsigned slen = samp->getDataByteLength();

    _processor->_totalRecords++;
    _processor->_recordsPerSecond++;


    // Use DSM time tags, since we don't have probe timestamps.
    dsm_time_t startTime = _processor->_prevTime;
    _processor->_prevTime = samp->getTimeTag();

    if (startTime == 0) return false;

    const unsigned char * eod = cp + slen;
    long long firstTimeWord = 0;

    // Restore any saved buffer from previous call.
    _processor->setupBuffer(&cp, &eod);


    uint16_t *wp = (uint16_t *)cp;
    int nImgWords;

    for (size_t j = 0; j < 2043; ++j)   // want at least 5 words, otherwise save
    {
        cp = (uint8_t *)&wp[j];

        if (wp[j] == _spec->FlushWord)          // NL flush buffer
        {
            // we want to make sure the buffer is discarded, there is no more data
            DLOG( ("NL flush @ idx = ") << j );
            _processor->createSamples(samp->getTimeTag(), results);
            eod = cp;
            _processor->saveBuffer(cp, eod);
            return !results.empty();
        }


        // Start of particle
        if ( _spec->isParticleSyncWord(&wp[j]) )
        {
            std::cout << " start of particle, j=" << j << " NH/NV=" << wp[j+1] << ", "
                    << wp[j+2] << std::endl;

            memcpy(_compressedParticle, &wp[j], 5 * sizeof(uint16_t));
            j += 5;
            nImgWords = _spec->extractNimageWords(_compressedParticle); // get number of image words to copy

            bool reject = false;
            if (nImgWords == 0 || nImgWords > 950) // seems runaway @ 960ish
                reject = true;

            // I am choosing not to deal with multi-packet particles.  If you do, then
            // make sure to understand that only the last packet has a timing word.
            if (_compressedParticle[_spec->ID] == _prevParticleID || _spec->_multiPacketParticle)
                reject = true;

            _prevParticleID = _compressedParticle[_spec->ID];

            if (reject)
            {
                j += nImgWords -1;
                continue;
            }

            if (j + nImgWords > 2048)   // Crosses into next buffer
            {
                // Save off and leave.
                DLOG( (" short image, j=") << j << ", n=" << nImgWords);
                break;
            }

            memcpy(&_compressedParticle[5], &wp[j], nImgWords * sizeof(uint16_t));
            j += (nImgWords-1);

            // Decompress particle and get stats.
            _processor->_particle.zero();
            _processor->_totalParticles++;
            size_t nSlices = _spec->decompressParticle(_compressedParticle, _uncompressedParticle);
            for (size_t k = 0; k < nSlices; ++k)
            {
                _processor->processParticleSlice(&_uncompressedParticle[k*16]);
            }

            // Get time
            long long thisTimeWord = ((unsigned long *)&_compressedParticle[nImgWords-3])[0] & _timingWordMask;
//                        (bigEndian->int64Value(cp) & _timeWordMask) /_probeClockRate;
// @TODO  equivelant of probe clock rate???
//   freq = probe.resolution / (1.0e6 * tas);


            if (firstTimeWord == 0)
                firstTimeWord = thisTimeWord;

            // Record time tag minus approx microseconds since start of record.
            long long thisParticleTime = startTime + (thisTimeWord - firstTimeWord);


            _processor->countParticle(0);
            _processor->createSamples(thisParticleTime, results);

        }
    }

    _processor->createSamples(samp->getTimeTag(), results);

    /* Data left in image block, save it in order to pre-pend to next image block */
    _processor->saveBuffer(cp, eod);

    return !results.empty();
}

/*---------------------------------------------------------------------------*/
bool TwoD_SPEC::processHousekeeping(const Sample * samp, list < const Sample * >&results)
{
    return !CharacterSensor::process(samp, results);
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
