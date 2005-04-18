/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_STATUSTHREAD_H
#define DSM_STATUSTHREAD_H

#include <atdUtil/Thread.h>

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

    int run() throw(atdUtil::Exception);

protected:
    /**
     * How often to run, in seconds.
     */
    int period;

};

}

#endif
