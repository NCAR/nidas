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

#include <Sample.h>
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
    StatusThread(const std::string& name);

    virtual int run() throw(atdUtil::Exception) = 0;

protected:
};

// ------------------

class DSMEngineStat: public StatusThread
{
public:
  DSMEngineStat(const std::string& name):StatusThread(name) {};

  int run() throw(atdUtil::Exception);
};

class DSMServerStat: public StatusThread
{
protected:
  /** The protected constructor, called from getInstance. */
  DSMServerStat(const std::string& name):StatusThread(name) {};

public:
  int run() throw(atdUtil::Exception);

  /**
   * Get a pointer to the singleton instance of DSMServerStat.
   * This will create the instance if it doesn't exist.
   */
  static DSMServerStat* getInstance();

  void setSomeTime(dsm_time_t time) { _sometime = time; };

private:
  static DSMServerStat* _instance;

  dsm_time_t _sometime;
};

}

#endif
