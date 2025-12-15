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

#include "TwoDS.h"
#include <nidas/core/Parameter.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, TwoDS)

const unsigned char TwoDS::_syncString[] = { 0xaa, 0xaa, 0xaa };
const unsigned char TwoDS::_blankString[] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

TwoDS::TwoDS() : TwoD_Processing("Fast2DS")
{

}

TwoDS::~TwoDS()
{

}


/*---------------------------------------------------------------------------*/
/* Initialization of things that are needed in real-time
 * and when post-processing.  Don't put stuff here that
 * is *only* needed during post-processing (the idea is to
 * save memory on DSMs).
 */
void TwoDS::init_parameters()
{
    const Parameter *p;

    // Acquire probe diode/pixel resolution (in micrometers) for tas encoding.
    p = getParameter("RESOLUTION");
    if (!p)
        throw n_u::InvalidParameterException(getName(), "RESOLUTION","not found");
    _resolutionMicron = (int)p->getNumericValue(0);
    _resolutionMeters = (float)_resolutionMicron * 1.0e-6;
}

/*---------------------------------------------------------------------------*/
/* Stuff that is necessary when post-processing.
 */
void TwoDS::init()
{
    UDPSocketSensor::init();
    init_parameters();

    // Find SampleID for 1D & 2D arrays.
    list<SampleTag *>& tags = getSampleTags();
    list<SampleTag *>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag * tag = *si;
        Variable & var = ((SampleTag *)tag)->getVariable(0);

        if (var.getName().compare(0, 3, "A1D") == 0) {
            _1dcID = tag->getId();
            _nextraValues = tag->getVariables().size() - 1;
        }

        if (var.getName().compare(0, 3, "A2D") == 0)
            _2dcID = tag->getId();
    }

    _prevTime = 0;

    _twoDAreaRejectRatio = 0.1;
    const Parameter * p = getParameter("AREA_RATIO_REJECT");
    if (p) {
        _twoDAreaRejectRatio = p->getNumericValue(0);
    }

    delete [] _size_dist_1D;
    delete [] _size_dist_2D;
    _size_dist_1D = new unsigned int[NumberOfDiodes()];
    _size_dist_2D = new unsigned int[NumberOfDiodes()<<1];
    clearData();
}

/*---------------------------------------------------------------------------*/
bool TwoDS::process(const Sample * samp, list < const Sample * >&results)
{
    unsigned slen = samp->getDataByteLength();
    const int wordSize = 16;
//    if (slen < sizeof (int32_t) + sizeof(Tap2D)) return false;

    _totalRecords++;
    _recordsPerSecond++;
return false;  // Remove when ready.

    // slen is coming in as 4121 bytes.  Actual record is 4114 [ts|image|cksum]
    // We can only print this if we are getting the same record as written to disk.
    unsigned short *ts = (unsigned short *)samp->getConstVoidDataPtr();
    std::cout << "TwoDS::processImageRecord, len = " << slen << " - " <<
                ts[0] << "/" << std::setw(2) << std::setfill('0') <<
                ts[1] << "/" << ts[3] << "  " <<
                ts[4] << ":" << ts[5] << ":" << ts[6] << "." << ts[7] << "\n";

    dsm_time_t startTime = _prevTime;
    _prevTime = samp->getTimeTag();

    if (startTime == 0) return false;

    const unsigned char * cp = (const unsigned char *) samp->getConstVoidDataPtr();
    const unsigned char * eod = cp + slen;

//    cp += sizeof(int16_t) * 8; // Move past timestamp?

    setupBuffer(&cp, &eod);

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
                    createSamples(samp->getTimeTag(), results);
                    saveBuffer(cp,eod);
                    return !results.empty();
                }
                if (cp[1] == _syncString[1]) {  // is a syncWord
                    _totalParticles++;
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
                processParticleSlice(_particle, sos);
            }
            cp = sos + wordSize;
        }
    }

    createSamples(samp->getTimeTag(), results);

    /* Data left in image block, save it in order to pre-pend to next image block */
    saveBuffer(cp,eod);

    return false;
}

/*---------------------------------------------------------------------------*/
void TwoDS::printStatus(std::ostream& ostr)
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
