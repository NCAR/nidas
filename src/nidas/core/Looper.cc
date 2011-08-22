/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/Looper.h>
#include <nidas/util/Logger.h>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

Looper::Looper(): n_u::Thread("Looper"),_sleepMsec(0)
{
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
    blockSignal(SIGUSR2);
}

void Looper::addClient(LooperClient* clnt, unsigned int msecPeriod)
	throw(n_u::InvalidParameterException)
{

    if (msecPeriod < 5) throw n_u::InvalidParameterException(
    	"Looper","addClient","requested callback period is too small ");
    // round to nearest 10 milliseconds, which better
    // matches the precision of the system nanosleep.
    msecPeriod += 5;
    msecPeriod = msecPeriod - msecPeriod % 10;

    n_u::Synchronized autoLock(_clientMutex);

    map<unsigned int,std::set<LooperClient*> >::iterator
    	ci = _clientsByPeriod.find(msecPeriod);

    if (ci != _clientsByPeriod.end()) ci->second.insert(clnt);
    else {
	/* new period value */
        set<LooperClient*> clnts;
	clnts.insert(clnt);
	_clientsByPeriod[msecPeriod] = clnts;
    }
    setupClientMaps();
}

void Looper::removeClient(LooperClient* clnt)
{
    _clientMutex.lock();

    map<unsigned int,std::set<LooperClient*> >::iterator
    	ci = _clientsByPeriod.begin();

    bool foundClient = false;
    for ( ; ci != _clientsByPeriod.end(); ++ci) {
        set<LooperClient*>::iterator si = ci->second.find(clnt);
	if (si != ci->second.end()) {
	    ci->second.erase(si);
	    foundClient = true;
	}
    }
    if (foundClient) setupClientMaps();
    bool haveClients = _cntrMods.size() > 0;
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
    map<unsigned int,std::set<LooperClient*> >::iterator ci;
    unsigned int sleepval = 0;

    /* determine greatest common divisor of periods */
    for (ci = _clientsByPeriod.begin(); ci != _clientsByPeriod.end(); ) {
        if (ci->second.size() == 0) _clientsByPeriod.erase(ci++);
	else {
	    unsigned int per = ci->first;
	    if (sleepval == 0) sleepval = per;
	    else sleepval = gcd(sleepval,per);
	    ++ci;
	}
    }

    _clientsByCntrMod.clear();
    _cntrMods.clear();
    if (sleepval == 0) return;      // no clients

    for (ci = _clientsByPeriod.begin(); ci != _clientsByPeriod.end(); ++ci) {
	assert (ci->second.size() > 0);
	unsigned int per = ci->first;
	assert((per % sleepval) == 0);
	int cntrMod = per / sleepval;
	list<LooperClient*> clnts(ci->second.begin(),ci->second.end());
	_clientsByCntrMod[cntrMod] = clnts;
	_cntrMods.insert(cntrMod);
    }
    _sleepMsec = sleepval;
    n_u::Logger::getInstance()->log(LOG_INFO,
	"Looper, sleepMsec=%d",_sleepMsec);

    if (!isRunning()) start();
}

/**
 * Thread function, the loop.
 */

int Looper::run() throw(n_u::Exception)
{
    if (sleepUntil(_sleepMsec)) return RUN_OK;

    n_u::Logger::getInstance()->log(LOG_INFO,
    	"Looper starting, sleepMsec=%d", _sleepMsec);
    while (!amInterrupted()) {
	dsm_time_t tnow = getSystemTime() / USECS_PER_MSEC;
	unsigned int cntr = (unsigned int)(tnow % MSECS_PER_DAY) / _sleepMsec;

	_clientMutex.lock();
	// make a copy of the list
	list<int> mods(_cntrMods.begin(),_cntrMods.end());
	_clientMutex.unlock();

	list<int>::const_iterator mi = mods.begin();
	for ( ; mi != mods.end(); ++mi) {
	    int modval = *mi;

	    if (!(cntr % modval)) {
		_clientMutex.lock();
		// make a copy of the list
		list<LooperClient*> clients = _clientsByCntrMod[modval];
		_clientMutex.unlock();

		list<LooperClient*>::const_iterator li = clients.begin();
		for ( ; li != clients.end(); ++li) {
		    LooperClient* clnt = *li;
		    clnt->looperNotify();
		}
	    }
	}
	if (sleepUntil(_sleepMsec)) return RUN_OK;
    }
    return RUN_OK;
}


