/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-12-05 19:38:10 -0700 (Mon, 05 Dec 2005) $

    $LastChangedRevision: 3187 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/PortSelector.cc $
 ********************************************************************
*/

#include <Looper.h>
#include <atdUtil/Logger.h>

using namespace std;
using namespace dsm;

/* static */
Looper* Looper::instance = 0;

/* static */
atdUtil::Mutex Looper::instanceMutex;

/* static */
Looper* Looper::getInstance()
{
    if (!instance) {
	atdUtil::Synchronized autosync(instanceMutex);
	if (!instance) instance = new Looper();
    }
    return instance;
}

/* static */
void Looper::removeInstance()
{
    instanceMutex.lock();
    delete instance;
    instance = 0;
    instanceMutex.unlock();
}
Looper::Looper(): atdUtil::Thread("Looper"),sleepUsec(0)
{
}

void Looper::addClient(LooperClient* clnt, int msecPeriod)
{
    atdUtil::Synchronized autoLock(clientMutex);

    unsigned long usecPeriod = msecPeriod * USECS_PER_MSEC;

    map<unsigned long,std::set<LooperClient*> >::iterator
    	ci = clientsByPeriod.find(usecPeriod);

    if (ci != clientsByPeriod.end()) ci->second.insert(clnt);
    else {
	/* new period value */
        set<LooperClient*> clnts;
	clnts.insert(clnt);
	clientsByPeriod[usecPeriod] = clnts;
    }
    setupClientMaps();
}

void Looper::removeClient(LooperClient* clnt)
{
    clientMutex.lock();

    map<unsigned long,std::set<LooperClient*> >::iterator
    	ci = clientsByPeriod.begin();

    bool haveClients = false;
    for ( ; ci != clientsByPeriod.end(); ++ci) {
        set<LooperClient*>::iterator si = ci->second.find(clnt);
	if (si != ci->second.end()) ci->second.erase(si);
	if (ci->second.size() > 0) haveClients = true;
    }
    clientsByCntrMod.clear();

    if (!haveClients) {
	clientsByCntrMod.clear();
	clientMutex.unlock();
        if (isRunning()) {
	    atdUtil::Logger::getInstance()->log(LOG_INFO,
		"No clients for Looper, doing cancel, join");
	    cancel();
	    join();
	}
	return;
    }
    setupClientMaps();
    clientMutex.unlock();
}

void Looper::setupClientMaps()
{
    unsigned long minperiod = 0;
    map<unsigned long,std::set<LooperClient*> >::iterator
    	ci = clientsByPeriod.begin();
    for ( ; ci != clientsByPeriod.end(); ++ci) {
	if (ci->second.size() == 0) continue;
	minperiod = ci->first;
	break;
    }
    assert(minperiod > 0);

    unsigned long sleepval = minperiod;
    int n = 1;

    for (;;) {
	/* determine lowest common denominator of periods */
	for (ci = clientsByPeriod.begin(); ci != clientsByPeriod.end();
		++ci) {
	    if (ci->second.size() == 0) continue;
	    unsigned long per = ci->first;
	    atdUtil::Logger::getInstance()->log(LOG_DEBUG,
	    	"Looper client period=%d",per);
	    if (per % sleepval) break;
	}
	if (ci == clientsByPeriod.end()) break;
	n++;
	sleepval = minperiod / n;
    }
    atdUtil::Logger::getInstance()->log(LOG_DEBUG,
    	"Looper client minperiod=%d, sleepval=%d",minperiod,sleepval);

    clientsByCntrMod.clear();
    for (ci = clientsByPeriod.begin(); ci != clientsByPeriod.end(); ++ci) {
	if (ci->second.size() == 0) continue;
	unsigned long per = ci->first;
	assert((per % sleepval) == 0);
	int cntrMod = per / sleepval;
	list<LooperClient*> clnts(ci->second.begin(),ci->second.end());
	clientsByCntrMod[cntrMod] = clnts;
	cntrMods.insert(cntrMod);
    }
    sleepUsec = sleepval;

    if (!isRunning()) start();
}

bool Looper::sleepTill(unsigned int periodUsec) throw(atdUtil::IOException)
{
    struct timespec sleepTime;
    /*
     * sleep until an even number of periodUsec since 
     * creation of the universe (Jan 1, 1970 0Z).
     */
    dsm_time_t tnow = getSystemTime();
    unsigned long uSecVal =
      periodUsec - (unsigned long)(tnow % periodUsec);

    sleepTime.tv_sec = uSecVal / USECS_PER_SEC;
    sleepTime.tv_nsec = (uSecVal % USECS_PER_SEC) * NSECS_PER_USEC;
    if (::nanosleep(&sleepTime,0) < 0) {
	if (errno == EINTR) return true;
	throw atdUtil::IOException("Looper","nanosleep",errno);
    }
    return false;
}

/**
 * Thread function, the loop.
 */

int Looper::run() throw(atdUtil::Exception)
{
    if (sleepTill(sleepUsec)) return RUN_OK;

    atdUtil::Logger::getInstance()->log(LOG_DEBUG,
    	"Looper starting, sleepUsec=%d", sleepUsec);
    while (!amInterrupted()) {

	/*
	 * maximum value of uSecSleep is 3600x10^6.
	 * minimum value is 10^4
	 * So max value for cntr is  86400*10^6/10^4 = 9*10^6
	 *    min value for cntr is  86400*10^6/3600*10^6 = 24
	 */
	dsm_time_t tnow = getSystemTime();
	int cntr = (tnow % USECS_PER_DAY) / sleepUsec;

	clientMutex.lock();
	// make a copy of the list
	list<int> mods(cntrMods.begin(),cntrMods.end());
	clientMutex.unlock();

	list<int>::const_iterator mi = mods.begin();
	for ( ; mi != mods.end(); ++mi) {
	    int modval = *mi;

	    if (!(cntr % modval)) {
		clientMutex.lock();
		// make a copy of the list
		list<LooperClient*> clients = clientsByCntrMod[modval];
		clientMutex.unlock();

		list<LooperClient*>::const_iterator li = clients.begin();
		for ( ; li != clients.end(); ++li) {
		    LooperClient* clnt = *li;
		    clnt->looperNotify();
		}
	    }
	}
	if (sleepTill(sleepUsec)) return RUN_OK;
    }
    return RUN_OK;
}


