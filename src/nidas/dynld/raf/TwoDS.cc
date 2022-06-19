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


TwoDS::TwoDS() :
    _numImages(0),_lastStatusTime(0),
    _resolutionMeters(0.0), _resolutionMicron(0),
    _1dcID(0), _2dcID(0),
    _size_dist_1D(0), _size_dist_2D(0),_dead_time(0.0),
    _totalRecords(0),_totalParticles(0),
    _rejected1D_Cntr(0), _rejected2D_Cntr(0),
    _overLoadSliceCount(0), _overSizeCount_2D(0),
    _misAligned(0),_suspectSlices(0),
    _recordsPerSecond(0), _totalPixelsShadowed(0),
    _prevTime(0),_histoEndTime(0),_twoDAreaRejectRatio(0.0),
    _particle(), _nextraValues(1),
    _saveBuffer(0),_savedBytes(0),_savedAlloc(0)
{
    setDefaultMode(O_RDWR);
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

/*---------------------------------------------------------------------------*/
void TwoDS::createSamples(dsm_time_t nextTimeTag,list < const Sample * >&results)
    throw()
{
    int nvalues;
    SampleT < float >*outs;
    float * dout;

    if (nextTimeTag < _histoEndTime) return;
    if (_histoEndTime == 0) {
        _histoEndTime = nextTimeTag + USECS_PER_SEC - (int)(nextTimeTag % USECS_PER_SEC);
        return;
    }

    if (_1dcID != 0) {
        // Sample 2 is the 1D entire-in data.
        nvalues = NumberOfDiodes() + _nextraValues;
        outs = getSample < float >(nvalues);

        // time tag is the start of the histogram
        outs->setTimeTag(_histoEndTime - USECS_PER_SEC);
        outs->setId(_1dcID);

        dout = outs->getDataPtr();
#ifdef ZERO_BIN_HACK
        // add a bogus zeroth bin for historical reasons
        *dout++ = 0.0;
#endif
        for (int i = 1; i < NumberOfDiodes(); ++i)
            *dout++ = (float)_size_dist_1D[i];

#ifndef ZERO_BIN_HACK
        *dout++ = 0.0;  // either at the beginning or the end....
#endif

        *dout++ = _dead_time / 1000;      // Dead Time, return milliseconds.
        if (_nextraValues > 1)
            *dout++ = _recordsPerSecond;

        if (_nextraValues > 2)
            *dout++ = (float)_totalPixelsShadowed * std::pow(1.0e-3 * _resolutionMicron, 2.0);

        results.push_back(outs);
    }

    // Sample 3 is the 2D center-in or reconstruction data.
    if (_2dcID != 0) {
        nvalues = (NumberOfDiodes()<<1) + 1;
        outs = getSample < float >(nvalues);

        // time tag is the start of the histogram
        outs->setTimeTag(_histoEndTime - USECS_PER_SEC);
        outs->setId(_2dcID);

        dout = outs->getDataPtr();
#ifdef ZERO_BIN_HACK
        // add a bogus zeroth bin for historical reasons
        *dout++ = 0.0;
#endif
        for (int i = 1; i < (NumberOfDiodes()<<1); ++i)
            *dout++ = (float)_size_dist_2D[i];

#ifndef ZERO_BIN_HACK
        *dout++ = 0.0;
#endif

        *dout++ = _dead_time / 1000;      // Dead Time, return milliseconds.
        results.push_back(outs);
    }

    clearData();

    // end time of next histogram
    _histoEndTime += USECS_PER_SEC;
    if (_histoEndTime <= nextTimeTag)
        _histoEndTime = nextTimeTag + USECS_PER_SEC - (int)(nextTimeTag % USECS_PER_SEC);
}

/*---------------------------------------------------------------------------*/
void TwoDS::processParticleSlice(Particle& p, const unsigned char * data)
{
    int nBytes = NumberOfDiodes() / 8;

    /* Note that 2D data is inverted.  So a '1' means no shadowing of the diode.
     * '0' means shadowing and a particle.  Perform complement here.
     */
    unsigned char slice[nBytes];
    for (int i = 0; i < nBytes; ++i)
        slice[i] = ~(data[i]);

    p.width++;

    if ((slice[0] & 0x80)) { // touched edge
        p.edgeTouch |= 0x0F;
    }

    if ((slice[nBytes-1] & 0x01)) { // touched edge
        p.edgeTouch |= 0xF0;
    }

    // Compute area = number of bits set in particle
    for (int i = 0; i < nBytes; ++i)
    {
        unsigned char c = slice[i];
        for (; c; p.area++)
            c &= c - 1; // clear the least significant bit set
    }

    // number of bits between first and last set bit, inclusive
    int h = NumberOfDiodes();
    for (int i = 0; i < nBytes; ++i)
    {
        if (slice[i] == 0)
        {
            h -= 8;
            continue;
        }
        int r = 7;
        unsigned char v = slice[i];
        while (v >>= 1)
            r--;
        h -= r;
        break;
    }
    for (int i = nBytes-1; i >= 0; --i)
    {
        if (slice[i] == 0)
        {
            h -= 8;
            continue;
        }
        int r = 0;
        unsigned char v = slice[i];
        while ((v & 0x01) == 0)
        {
            r++;
            v >>= 1;
        }
        h -= r;
        break;
    }

    if (h > 0)
        p.height = std::max((unsigned)h, p.height);
}

/*---------------------------------------------------------------------------*/
bool TwoDS::acceptThisParticle1D(const Particle& p) const
{
    if (p.dofReject)
        return false;

    if (p.edgeTouch || p.height == 0 ||
        (p.height == 1 && p.width > 3)) // Stuck bit.
        return false;

    if ((float)p.area / (std::pow(std::max(p.width, p.height), 2.0) * M_PI / 4.0) <= _twoDAreaRejectRatio)
        return false;

    return true;
}

bool TwoDS::acceptThisParticle2D(const Particle& p) const
{
    if (p.dofReject)
        return false;

    if (p.height == 1 && p.width > 3) // Stuck bit.
        return false;

    if ((float)p.area / (std::pow(std::max(p.width, p.height), 2.0) * M_PI / 4.0) <= _twoDAreaRejectRatio)
        return false;

    if (p.edgeTouch && p.width > p.height * 2)	// Center-in
        return false;

    return true;
}


/*---------------------------------------------------------------------------*/
void TwoDS::countParticle(const Particle& p, float /* resolutionUsec */)
{
    static n_u::LogContext sdlog(LOG_VERBOSE, "slice_debug");
    static n_u::LogMessage sdmsg(&sdlog);

    // 1D
    if (acceptThisParticle1D(p))
    {
        _size_dist_1D[p.height]++;
        _totalPixelsShadowed += p.area;
    }
    else
    {
        // float liveTime = resolutionUsec * p.width;
        _rejected1D_Cntr++;
    }

    // 2D - Center-in algo
    if (acceptThisParticle2D(p))
    {
        int n = std::max(p.height, p.width);
        if (n < (NumberOfDiodes()<<1))
            _size_dist_2D[n]++;
        else
            _overSizeCount_2D++;
    }
    else
    {
        // float liveTime = resolutionUsec * p.width;
        _rejected2D_Cntr++;
    }

    if (sdlog.active())
    {
        sdmsg << "1D: [";
        stream_histogram(sdmsg, _size_dist_1D, NumberOfDiodes());
        sdmsg << "]; reject=" << _rejected1D_Cntr << n_u::endlog;
        sdmsg << "2D: [";
        stream_histogram(sdmsg, _size_dist_2D, NumberOfDiodes() << 1);
        sdmsg << "]; reject=" << _rejected2D_Cntr
              << ", oversize=" << _overSizeCount_2D << n_u::endlog;
    }
}

/*---------------------------------------------------------------------------*/
void TwoDS::clearData()
{
    ::memset(_size_dist_1D, 0, NumberOfDiodes()*sizeof(unsigned int));
    ::memset(_size_dist_2D, 0, NumberOfDiodes()*sizeof(unsigned int)*2);

    _dead_time = 0.0;
    _recordsPerSecond = 0;
    _totalPixelsShadowed = 0;
}

void TwoDS::setupBuffer(const unsigned char** cp,const unsigned char** eod)
{
    if (_savedBytes > 0) {
        int lrec = *eod - *cp;
        int l = _savedBytes + lrec;
        if (_savedAlloc < l) {
            unsigned char* newBuffer = new unsigned char[l];
            if (_savedBytes > 0)
                ::memcpy(newBuffer,_saveBuffer,_savedBytes);
            delete [] _saveBuffer;
            _saveBuffer = newBuffer;
            _savedAlloc = l;
        }
        ::memcpy(_saveBuffer+_savedBytes,*cp,lrec);
        *cp = _saveBuffer;
        *eod = _saveBuffer + _savedBytes + lrec;
    }
}

void TwoDS::saveBuffer(const unsigned char* cp, const unsigned char* eod)
{
    assert(eod >= cp);
    int lrec = eod - cp;
#ifdef DEBUG
    if (lrec > 0) cerr << "saving " << lrec << endl;
#endif
    if (_savedAlloc < lrec) {
        // cp may point into _saveBuffer
        unsigned char* newBuffer =  new unsigned char[lrec];
        ::memcpy(newBuffer,cp,lrec);
        delete [] _saveBuffer;
        _saveBuffer = newBuffer;
        _savedAlloc = lrec;
    }
    else if (lrec > 0) ::memmove(_saveBuffer,cp,lrec);
    _savedBytes = lrec;
}

