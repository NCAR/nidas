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

#include "/home/local/SPECINC/SPEC-RLE.hh"


using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

// This will ve a Type32 format, not Type48
const unsigned char TwoD_SPEC::_syncString[] = { 0xaa, 0xaa };
const unsigned char TwoD_SPEC::_blankString[] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


TwoD_SPEC::TwoD_SPEC(std::string name) : _name(name), _processor(0)
{
  _processor = new TwoD_Processing(name, NumberOfDiodes(), this);

}

TwoD_SPEC::~TwoD_SPEC()
{
  delete _processor;
}


/*---------------------------------------------------------------------------*/
void TwoD_SPEC::init()
{
    UDPSocketSensor::init();
    _processor->init();
}


bool TwoD_SPEC::process(const Sample * samp, list < const Sample * >&results)
{
    unsigned slen = samp->getDataByteLength();
    const int wordSize = 16;
//    if (slen < sizeof (int32_t) + sizeof(Tap2D)) return false;

    _processor->_totalRecords++;
    _processor->_recordsPerSecond++;
return false;  // Remove when ready.

    // slen is coming in as 4121 bytes.  Actual record is 4114 [ts|image|cksum]
    // We can only print this if we are getting the same record as written to disk.
    unsigned short *ts = (unsigned short *)samp->getConstVoidDataPtr();
    std::cout << _name << "::processImageRecord, len = " << slen << " - " <<
                ts[0] << "/" << std::setw(2) << std::setfill('0') <<
                ts[1] << "/" << ts[3] << "  " <<
                ts[4] << ":" << ts[5] << ":" << ts[6] << "." << ts[7] << "\n";

    dsm_time_t startTime = _processor->_prevTime;
    _processor->_prevTime = samp->getTimeTag();

    if (startTime == 0) return false;

    const unsigned char * cp = (const unsigned char *) samp->getConstVoidDataPtr();
    const unsigned char * eod = cp + slen;

//    cp += sizeof(int16_t) * 8; // Move past timestamp?

    _processor->setupBuffer(&cp, &eod);

    // Loop through all slices in record.

// quiet compiler warning until we are ready to complete this function.
//    long long firstTimeWord = 0;        // First timing word in this record.

    for (; cp < eod - (wordSize - 1); )
    {
        /* Four cases, syncWord, overloadWord, blank or legitimate slice.
         * sync & overload words come at the end of the particle.  In the
         * case of the this probe, the time word is embedded in the sync
         * and overload word.
         */

        // possible start of particle slice if it isn't a sync or overload word
        const unsigned char* sos = cp;

        /* Scan next 8 bytes starting at current pointer, cp, for
         * a possible syncWord or overloadWord */
        const unsigned char* eow = cp + wordSize;

        for (; cp < eow; ++cp) {
            if (*cp == 0xaa) { // start of possible particle string
                if (cp + wordSize > eod) {
                    _processor->createSamples(samp->getTimeTag(), results);
                    _processor->saveBuffer(cp,eod);
                    return !results.empty();
                }
                if (cp[1] == _syncString[1]) {  // is a syncWord
                    _processor->_totalParticles++;
/*

                    if (firstTimeWord == 0)
                        firstTimeWord = thisTimeWord;

                    // Approx microseconds since start of record.
                    long long thisParticleTime = startTime + (thisTimeWord - firstTimeWord);


                    // This is incomplete, needs to be fully flushed out.
                    // See TwoD64_USB.cc  This will be the same but different.
*/
                }
            }
        }
        if (sos) {
            // scan 8 bytes of particle
            // If a blank string, then next word should be sync, otherwise discard it
            if (::memcmp(sos,_blankString,sizeof(_blankString)) != 0) {
                _processor->processParticleSlice(_processor->_particle, sos);
            }
            cp = sos + wordSize;
        }
    }

    _processor->createSamples(samp->getTimeTag(), results);

    /* Data left in image block, save it in order to pre-pend to next image block */
    _processor->saveBuffer(cp,eod);

    return false;
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
