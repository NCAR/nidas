// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

#include "Looper.h"
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

Looper::Looper():
    n_u::Thread("Looper"),
    _clientMutex(), _clients(),
    _clientPeriods(), _clientOffsets(),
    _clientDivs(), _clientMods(),
    _sleepMsec(0)
{
}

void Looper::addClient(LooperClient* clnt, unsigned int msecPeriod,
        unsigned int msecOffset)
	throw(n_u::InvalidParameterException)
{

    if (msecPeriod < 5) throw n_u::InvalidParameterException(
    	"Looper","addClient","requested callback period is too small ");
    // round to nearest 10 milliseconds, which better
    // matches the precision of the system nanosleep.
    msecPeriod += 5;
    msecPeriod = msecPeriod - msecPeriod % 10;

    n_u::Synchronized autoLock(_clientMutex);

    _clientPeriods[clnt] = msecPeriod;
    _clientOffsets[clnt] = msecOffset;

    setupClientMaps();
}

void Looper::removeClient(LooperClient* clnt)
{
    _clientMutex.lock();
    map<LooperClient*, unsigned int>::iterator ci = _clientPeriods.find(clnt);
    if (ci != _clientPeriods.end()) _clientPeriods.erase(ci);

    ci = _clientOffsets.find(clnt);
    if (ci != _clientOffsets.end()) _clientOffsets.erase(ci);

    setupClientMaps();

    bool haveClients = !_clients.empty();
    _clientMutex.unlock();

    if (!haveClients && isRunning()) {
	interrupt();
	if (Thread::currentThreadId() != getId()) {
	    n_u::Logger::getInstance()->log(LOG_INFO,
		"Interrupted Looper, doing join");
	    join();
	}
    }
}

/* Use Euclidian resursive algorimthm to find greatest common divisor.
 * Thanks to Wikipedia. */
int Looper::gcd(unsigned int a, unsigned int b)
{
    if (b == 0) return a;
    return gcd(b,a % b);
}

void Looper::setupClientMaps()
{
    map<LooperClient*, unsigned int>::iterator ci;
    unsigned int sleepval = 0;

    /* determine greatest common divisor of periods */
    for (ci = _clientPeriods.begin(); ci != _clientPeriods.end(); ) {
        unsigned int per = ci->second;
        if (sleepval == 0) sleepval = per;
        else sleepval = gcd(sleepval,per);
        ++ci;
    }
    for (ci = _clientOffsets.begin(); ci != _clientOffsets.end(); ) {
        unsigned int off = ci->second;
        if (off != 0) sleepval = gcd(sleepval,off);
        ++ci;
    }

    _clientDivs.clear();
    _clientMods.clear();
    _clients.clear();
    if (sleepval == 0) return;      // no clients

    for (ci = _clientPeriods.begin(); ci != _clientPeriods.end(); ++ci) {
        LooperClient* clnt = ci->first;
	unsigned int per = ci->second;
	unsigned int offset = _clientOffsets[clnt];

        DLOG(("sleepval=%u, per=%u offset=%u",
                    sleepval, per, offset));

	assert((per % sleepval) == 0);
	_clientDivs[clnt] = per / sleepval;

	assert((offset % sleepval) == 0);
	_clientMods[clnt] = offset / sleepval;

        // add client to list so that fastest clients are called
        // first, in the order that they registered.
        list<LooperClient*>::iterator cli = _clients.begin();
        for ( ; cli != _clients.end(); ++cli) {
            if (_clientPeriods[*cli] > per) break;
        }
        _clients.insert(cli, clnt);
    }

    assert(_clients.size() == _clientPeriods.size());
    assert(_clients.size() == _clientOffsets.size());

    _sleepMsec = sleepval;
    ILOG(("Looper, sleepMsec=%d",_sleepMsec));


    if (!isRunning()) start();
}

/*
 * Thread function, the loop.
 */
int Looper::run() throw(n_u::Exception)
{
    if (n_u::sleepUntil(_sleepMsec)) return RUN_OK;

    ILOG(("Looper starting, sleepMsec=%d", _sleepMsec));
    while (!amInterrupted()) {
	unsigned int tnow =
            (unsigned int)(n_u::getSystemTime() / USECS_PER_MSEC % MSECS_PER_DAY);
	unsigned int cntr = (tnow + _sleepMsec / 2) / _sleepMsec;

	_clientMutex.lock();
	// make a copy of the list
	list<LooperClient*> clnts(_clients.begin(),_clients.end());
	_clientMutex.unlock();

        // If more than one client are to be called on this
        // wakeup, fastest clients are called first, and if
        // more than one at the same rate, in the order that they
        // registered with Looper.
	list<LooperClient*>::const_iterator ci = clnts.begin();
	for ( ; ci != clnts.end(); ++ci) {
	    LooperClient* clnt = *ci;

            _clientMutex.lock();
            unsigned int cdiv = _clientDivs[clnt];
            unsigned int cmod = _clientMods[clnt];
            _clientMutex.unlock();
            VLOG(("cntr=%u, cdiv=%u cmod=%u",
                    cntr, cdiv, cmod));
            if (cdiv > 0 && (cntr % cdiv) == cmod) clnt->looperNotify();
	}
	if (n_u::sleepUntil(_sleepMsec)) return RUN_OK;
    }
    return RUN_OK;
}

