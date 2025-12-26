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

#include "TwoD_USB.h"
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

// 23 m/s mimics the newer spinning disk. 33 for the older.
const float TwoD_USB::DefaultTrueAirspeed = 23.0;

const n_u::EndianConverter * TwoD_USB::bigEndian =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::
                                       EC_BIG_ENDIAN);

const n_u::EndianConverter * TwoD_USB::littleEndian =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::
                                       EC_LITTLE_ENDIAN);

TwoD_USB::TwoD_USB(std::string name) : _name(name), _processor(0), _tasRate(1), _tasOutOfRange(0), _sorID(0), _trueAirSpeed(0)
{
    setDefaultMode(O_RDWR);
}

TwoD_USB::~TwoD_USB()
{
}

IODevice *TwoD_USB::buildIODevice()
{
    return new UnixIODevice();
}

SampleScanner *TwoD_USB::buildSampleScanner()
{
    return new DriverSampleScanner((4104 + 8) * 4);
}


/*---------------------------------------------------------------------------*/
void TwoD_USB::open(int flags)
{
    DSMSensor::open(flags);
    init_parameters();

    // Shut the probe down until a valid TAS comes along.
    sendTrueAirspeed(DefaultTrueAirspeed);

    // cerr << "SET_SOR_RATE, rate="<<_tasRate<<endl;
    ioctl(USB2D_SET_SOR_RATE, (void *) &_tasRate, sizeof (int));

    if (DerivedDataReader::getInstance())
        DerivedDataReader::getInstance()->addClient(this);
    else
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
		getName().c_str(),
		"no DerivedDataReader. <dsm> tag needs a derivedData attribute");
}

void TwoD_USB::close()
{
    if (DerivedDataReader::getInstance())
	    DerivedDataReader::getInstance()->removeClient(this);
    DSMSensor::close();
}

/*---------------------------------------------------------------------------*/
/* Initialization of things that are needed in real-time
 * and when post-processing.  Don't put stuff here that
 * is *only* needed during post-processing (the idea is to
 * save memory on DSMs).
 */
void TwoD_USB::init_parameters()
{
    const Parameter *p;

    p = getParameter("TAS_RATE");
    if (!p)
        throw n_u::InvalidParameterException(getName(), "TAS_RATE","not found");
    setTASRate((int)(rint(p->getNumericValue(0)))); //tas_rate is the same rate used as sor_rate
}

/*---------------------------------------------------------------------------*/
/* Stuff that is necessary when post-processing.
 */
void TwoD_USB::init()
{
    DSMSensor::init();
    _processor = new TwoD_Processing(_name, NumberOfDiodes(), this);
    _processor->init();
    init_parameters();

}

/*---------------------------------------------------------------------------*/
void TwoD_USB::derivedDataNotify(const nidas::core::DerivedDataReader * s)
{
    // std::cerr << "tas " << s->getTrueAirspeed() << std::endl;
    _trueAirSpeed = s->getTrueAirspeed();   // save it to display in printStatus
    if (!::isnan(_trueAirSpeed)) {
	try {
	    sendTrueAirspeed(_trueAirSpeed);
	}
	catch(const n_u::IOException & e)
	{
	    n_u::Logger::getInstance()->log(LOG_WARNING, "%s", e.what());
	}
    }
}

/*---------------------------------------------------------------------------*/
int TwoD_USB::TASToTap2D(void * tap2d, float tas)
{
   Tap2D * t2d = (Tap2D*)tap2d;
    /* Default tas to spinning disk speed if we are not moving.  This
     * will probably bite us some day when they try to use a 2D probe on
     * ISF or ISFF....
     */
    if (tas < DefaultTrueAirspeed)
        tas = DefaultTrueAirspeed;

    double freq = tas / _processor->getResolution();
    double maxfreq;
    double PotFudgeFactor = 1.01;

    memset(t2d, 0, sizeof(*t2d));

    /*
     * Minimum frequency we can generate is either:
     *
     *   2 MHz (with no frequency divider)
     *      OR
     *   300 kHz (using frequency divider factor 10)
     */
    if (freq >= 2.0e6) {
        t2d->div10 = 0;
        maxfreq = 1.0e11;
    }
    else if (freq >= 3.0e5) {
        t2d->div10 = 1;  // set the divide-by-ten flag
        maxfreq = 1.0e10;
    }
    else {
    /*
     * Desired frequency is too low.  Fill the struct to generate
     * the lowest possible frequency and return -EINVAL to let the
     * caller know that the TAS is too low.
     */
        t2d->ntap = 0;
        t2d->div10 = 1;
        return -EINVAL;
    }

    float x = (511.0 - ((maxfreq / freq) * 511.0 / 25000.0 / 2.0)) *
		PotFudgeFactor + 0.5;
    t2d->ntap = (unsigned short)x;
//    t2d->ntap = (unsigned short)(511 - ((maxfreq / freq) * 511 / 25000 / 2));

    return 0;               /* Return success */
}

/*---------------------------------------------------------------------------*/
float TwoD_USB::Tap2DToTAS(const Tap2D * t2d) const
{
    float tas = (1.0e11 / ((float)t2d->ntap * 2 * 25000 / 511)) * _processor->getResolution();

    if (t2d->div10 == 1)
        tas /= 10.0;

    return tas;
}

/*---------------------------------------------------------------------------*/
float TwoD_USB::Tap2DToTAS(const Tap2Dv1 * t2d) const
{
    float tas = (1.0e6 / (1.0 - ((float)t2d->ntap / 255))) * _processor->getResolution();

    if (t2d->div10 == 1)
        tas /= 10.0;

    return tas;
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::sendTrueAirspeed(float tas)
{
    Tap2D tx_tas;
    if (TASToTap2D(&tx_tas, tas))
	n_u::Logger::getInstance()->log(LOG_WARNING,
            "%s: TASToTap2D reports bad airspeed=%f m/s",
		getName().c_str(),tas);

    ioctl(USB2D_SET_TAS, (void *) &tx_tas, sizeof (Tap2D));
}

/*---------------------------------------------------------------------------*/
void TwoD_USB::printStatus(std::ostream& ostr)
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td></tr>" << endl;
	return;
    }
    struct usb_twod_stats status;

    try {
	ioctl(USB2D_GET_STATUS,&status,sizeof(status));
	long long tnow = n_u::getSystemTime();
	float imagePerSec = float(status.numImages - _processor->_numImages) /
		float(tnow - _processor->_lastStatusTime) * USECS_PER_SEC;
	_processor->_numImages = status.numImages;
	_processor->_lastStatusTime = tnow;

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
}
