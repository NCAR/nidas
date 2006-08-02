/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-23 12:30:55 -0600 (Tue, 23 May 2006) $

    $LastChangedRevision: 3364 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/dynld/SimGOESXmtr.cc $

 ******************************************************************
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

#define DEBUG

NIDAS_CREATOR_FUNCTION_NS(isff,SimGOESXmtr)

SimGOESXmtr::SimGOESXmtr()
{
}

SimGOESXmtr::SimGOESXmtr(const SimGOESXmtr& x): GOESXmtr(x)
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

void SimGOESXmtr::transmitData(const n_u::UTime& at, int configid,
	const Sample* samp) throw(n_u::IOException)
{
    transmitQueueTime = n_u::UTime();
    transmitAtTime = at;
    transmitSampleTime = samp->getTimeTag();
    cerr << "transmitData: " << at.format(true,"%c") << endl;
}

void SimGOESXmtr::printStatus() throw()
{
    cout << "Sim GOES Transmitter\n" <<
	"dev=\t" << port.getName() << '\n' <<
    	"model=\t" << 99 << '\n' <<
	"id=\t" << hex << setw(8) << setfill('0') << 0 << dec << '\n' <<
	"channel\t=" << getChannel() << '\n' <<
	"rfBaud=\t" << getRFBaud() << '\n' <<
	"xmit interval=\t" << getXmitInterval() << '\n' <<
	"xmit offset=\t" << getXmitOffset() << '\n' <<
	"xmitr clock diff=" << 0 << " msecs ahead of UNIX clock\n" <<
    cout << "xmit queued at=\t" << transmitQueueTime.format(true,"%c") << '\n' <<
    cout << "xmit time=\t" << transmitAtTime.format(true,"%c") << '\n' <<
    cout << "sample time=\t" << transmitSampleTime.format(true,"%c") << '\n' <<
    cout << "last status=\t" << "OK" << '\n' << endl;
}


