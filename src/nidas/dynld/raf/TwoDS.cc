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


TwoDS::TwoDS()
{

}

TwoDS::~TwoDS()
{
    delete [] _size_dist_1D;
    delete [] _size_dist_2D;

    if (_totalRecords > 0) {
        std::cerr << "Total number of 2D records = " << _totalRecords << std::endl;
        std::cerr << "Total number of 2D particles detected = " << _totalParticles << std::endl;
        std::cerr << "Number of rejected particles for 1D = " << _rejected1D_Cntr << std::endl;
        std::cerr << "Number of rejected particles for 2D = " << _rejected2D_Cntr << std::endl;
        std::cerr << "Number of overload words = " << _overLoadSliceCount << std::endl;
        std::cerr << "2D over-sized particle count = " << _overSizeCount_2D << std::endl;
        std::cerr << "Number of misaligned sync words = " << _misAligned << std::endl;
        std::cerr << "Number of suspect slices = " << _suspectSlices << std::endl;
    }
    delete [] _saveBuffer;
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
bool TwoDS::processHousekeeping(const Sample * samp, list < const Sample * >&results)
{
    return !CharacterSensor::process(samp, results);
}

bool TwoDS::processImageRecord(const Sample * samp, list < const Sample * >&results)
{
    return false;
}

/*---------------------------------------------------------------------------*/
bool TwoDS::process(const Sample * samp, list < const Sample * >&results)
{
    const char *input = (const char*) samp->getConstVoidDataPtr();
    bool result = false;

    if (!strncmp(input, "SPEC2D,", 7))
        result = processHousekeeping(samp, results);    // len == ~250
    else
        result = processImageRecord(samp, results); // len == 4121

    return result;
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
