
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-12-02 10:42:20 -0700 (Fri, 02 Dec 2005) $

    $LastChangedRevision: 3166 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/SensorHandler.h $
 ********************************************************************

*/


#ifndef NIDAS_CORE_LOOPER_H
#define NIDAS_CORE_LOOPER_H

#include <nidas/core/DSMTime.h>
#include <nidas/core/LooperClient.h>

#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/IOException.h>

#include <sys/time.h>
#include <sys/select.h>

namespace nidas { namespace core {

/**
 * Looper is a Thread that periodically loops, calling
 * the LooperClient::looperNotify() method of
 * LooperClients at the requested intervals.
 */
class Looper : public nidas::util::Thread {
public:

    /**
     * Fetch the pointer to the instance of Looper
     */
    static Looper* getInstance();

    /**
     * Delete the instance of Looper. Probably only done
     * at main program shutdown, and then only if you're really
     * worried about cleaning up.
     */
    static void removeInstance();

    /**
     * Add a client to the Looper whose
     * LooperClient::looperNotify() method should be
     * called every msec number of milliseconds.
     * @param msec Time period, in milliseconds.
     */
    void addClient(LooperClient *clnt,int msecPeriod);

    /**
     * Remove a client from the Looper.
     */
    void removeClient(LooperClient *clnt);

    /**
     * Thread function.
     */
    virtual int run() throw(nidas::util::Exception);

    /**
     * Utility function, sleeps until the next even period + offset.
     */
    static bool sleepUntil(unsigned int periodUsec,unsigned int offsetUsec=0)
    	throw(nidas::util::IOException);

private:

    void setupClientMaps();

    /**
     * Constructor.
     */
    Looper();

    static Looper* instance;

    static nidas::util::Mutex instanceMutex;

    nidas::util::Mutex clientMutex;

    std::map<unsigned long,std::set<LooperClient*> > clientsByPeriod;

    std::map<int,std::list<LooperClient*> > clientsByCntrMod;

    std::set<int> cntrMods;

    unsigned long sleepUsec;
};

}}	// namespace nidas namespace core

#endif
