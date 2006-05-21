
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-12-02 10:42:20 -0700 (Fri, 02 Dec 2005) $

    $LastChangedRevision: 3166 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/PortSelector.h $
 ********************************************************************

*/


#ifndef DSM_LOOPER_H
#define DSM_LOOPER_H

#include <DSMTime.h>
#include <LooperClient.h>

#include <atdUtil/Thread.h>
#include <atdUtil/ThreadSupport.h>
#include <atdUtil/IOException.h>

#include <sys/time.h>
#include <sys/select.h>

namespace dsm {

/**
 * Looper is a Thread that periodically loops, calling
 * the LooperClient::looperNotify() method of
 * LooperClients at the requested intervals.
 */
class Looper : public atdUtil::Thread {
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
    virtual int run() throw(atdUtil::Exception);

    /**
     * Utility function, sleeps until the next even period.
     */
    static bool sleepTill(unsigned int periodUsec) throw(atdUtil::IOException);

private:

    void setupClientMaps();

    /**
     * Constructor.
     */
    Looper();

    static Looper* instance;

    static atdUtil::Mutex instanceMutex;

    atdUtil::Mutex clientMutex;

    std::map<unsigned long,std::set<LooperClient*> > clientsByPeriod;

    std::map<int,std::list<LooperClient*> > clientsByCntrMod;

    std::set<int> cntrMods;

    unsigned long sleepUsec;
};
}
#endif
