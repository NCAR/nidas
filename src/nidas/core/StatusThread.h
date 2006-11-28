/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_STATUSTHREAD_H
#define NIDAS_CORE_STATUSTHREAD_H

#include <nidas/core/Sample.h>
#include <nidas/util/Thread.h>

#include <iostream> // cerr

namespace nidas { namespace core {

/**
 * A thread that runs periodically checking and multicasting
 * the status of a DSMEngine.
 */
class StatusThread: public nidas::util::Thread
{
public:
    /**
     * Constructor.
     */
    StatusThread(const std::string& name);

    virtual int run() throw(nidas::util::Exception) = 0;

protected:
};

// ------------------

class DSMEngineStat: public StatusThread
{
public:
    DSMEngineStat(const std::string& name):StatusThread(name) {};

    int run() throw(nidas::util::Exception);
};

class DSMServerStat: public StatusThread
{
protected:
    /** The protected constructor, called from getInstance. */
    DSMServerStat(const std::string& name);

public:
    int run() throw(nidas::util::Exception);

    /**
    * Get a pointer to the singleton instance of DSMServerStat.
    * This will create the instance if it doesn't exist.
    */
    static DSMServerStat* getInstance();

    void setSomeTime(dsm_time_t time) { _sometime = time; };

private:
    static DSMServerStat* _instance;

    dsm_time_t _sometime;

    /**
     * Wakeup period.
     */
    int uSecPeriod;
};

}}	// namespace nidas namespace core

#endif
