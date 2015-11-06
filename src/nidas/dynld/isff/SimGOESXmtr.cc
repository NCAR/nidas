/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include <nidas/dynld/isff/SimGOESXmtr.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/dynld/isff/GOESException.h>
#include <nidas/util/UTime.h>

#include <nidas/util/Logger.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;
using namespace nidas::dynld::isff;

namespace n_u = nidas::util;

#ifndef DEBUG
#define DEBUG
#endif

NIDAS_CREATOR_FUNCTION_NS(isff,SimGOESXmtr)

SimGOESXmtr::SimGOESXmtr():
    _transmitQueueTime((time_t)0),
    _transmitAtTime((time_t)0),
    _transmitSampleTime((time_t)0),
    _rfBaud(0)
{
}

SimGOESXmtr::SimGOESXmtr(const SimGOESXmtr& x):
    GOESXmtr(x),
    _transmitQueueTime((time_t)0),
    _transmitAtTime((time_t)0),
    _transmitSampleTime((time_t)0),
    _rfBaud(x._rfBaud)
{
}

SimGOESXmtr::~SimGOESXmtr()
{
}

void SimGOESXmtr::open() throw(n_u::IOException)
{
}


unsigned long SimGOESXmtr::checkId() throw(n_u::IOException)
{
    return 0;
}

int SimGOESXmtr::checkClock() throw(n_u::IOException)
{
    return 0;
}

void SimGOESXmtr::transmitData(const n_u::UTime& at, int /* configid */,
	const Sample* samp) throw(n_u::IOException)
{
    _transmitQueueTime = n_u::UTime();
    _transmitAtTime = at;
    _transmitSampleTime = samp->getTimeTag();
    cerr << "transmitData: " << at.format(true,"%c") << endl;
}

void SimGOESXmtr::printStatus() throw()
{
    cout << "Sim GOES Transmitter\n" <<
	"dev=\t" << _port.getName() << '\n' <<
    	"model=\t" << 99 << '\n' <<
	"id=\t" << hex << setw(8) << setfill('0') << 0 << dec << '\n' <<
	"channel\t=" << getChannel() << '\n' <<
	"rfBaud=\t" << getRFBaud() << '\n' <<
	"xmit interval=\t" << getXmitInterval() << '\n' <<
	"xmit offset=\t" << getXmitOffset() << '\n' <<
	"xmitr clock diff=" << 0 << " msecs ahead of UNIX clock\n" <<
    cout << "xmit queued at=\t" << _transmitQueueTime.format(true,"%c") << '\n' <<
    cout << "xmit time=\t" << _transmitAtTime.format(true,"%c") << '\n' <<
    cout << "sample time=\t" << _transmitSampleTime.format(true,"%c") << '\n' <<
    cout << "last status=\t" << "OK" << '\n' << endl;
}


