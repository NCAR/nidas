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
public:
    /** The protected constructor, called from getInstance. */
    DSMServerStat(const std::string& name);

    int run() throw(nidas::util::Exception);

private:

    void setup();

    /**
     * Wakeup period.
     */
    int _uSecPeriod;
};

}}	// namespace nidas namespace core

#endif
