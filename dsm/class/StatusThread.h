/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_STATUSTHREAD_H
#define DSM_STATUSTHREAD_H

#include <atdUtil/Thread.h>

#include <iostream> // cerr

namespace dsm {

/**
 * A thread that runs periodically checking and multicasting
 * the status of a DSMEngine.
 */
class StatusThread: public atdUtil::Thread
{
public:
    
    /**
     * Constructor.
     */
    StatusThread(const std::string& name,int runPeriod);

    virtual ~StatusThread() {
      std::cerr << __FUNCTION__ << std::endl;
      if (isRunning()) {
        cancel();
        join();
        std::cerr << __FUNCTION__ << " canceled...joined..." << std::endl;
      }
    }
    int run() throw(atdUtil::Exception);

    // DEBUG trace!
    virtual void interrupt() {
      std::cerr << "StatusThread:" << __FUNCTION__ << std::endl;
      Thread::interrupt();
    }

    // DEBUG trace!
    virtual void start() throw (atdUtil::Exception) {
      std::cerr << "StatusThread:" << __FUNCTION__ << std::endl;
      Thread::start();
    }

protected:
    /**
     * How often to run, in seconds.
     */
    int period;

};

}

#endif
