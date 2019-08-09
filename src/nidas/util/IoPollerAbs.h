// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2013, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_UTIL_IOPOLLERABS_H
#define NIDAS_UTIL_IOPOLLERABS_H

#include "IoPollerIf.h"
#include "time_constants.h"

#include "Logger.h"

#include <sys/signal.h>
#include <sys/time.h>
#include <pthread.h>

namespace nidas { namespace util {

/**
 * Interface for objects with a file descriptor, providing
 * virtual methods to add/remove/poll for such system polling
 * methods as select, poll, or epoll.
 */
class IoPollerAbs : public IoPollerIf
{
public:
    IoPollerAbs() : IoPollerIf(), _sigmask(), _timeout() 
    {
        changeTimeout(0);
    }

    IoPollerAbs(int msecTimeout) : IoPollerIf(), _sigmask(), _timeout() 
    {
        changeTimeout(msecTimeout);
    }

    virtual ~IoPollerAbs() {}

    virtual void changeTimeout(int msecTimeout)
    {
        _timeout.tv_sec = msecTimeout/MSECS_PER_SEC;
        _timeout.tv_nsec = (msecTimeout%MSECS_PER_SEC) * NSECS_PER_SEC;
    }

    void addSignals(unsigned long how, unsigned long int signals)
    {
        pthread_sigmask(how, 0, &_sigmask);
        sigaddset(&_sigmask, signals);
    }

    void removeSignals(unsigned long int how, unsigned long int signals)
    {
        pthread_sigmask(how, 0, &_sigmask);
        sigdelset(&_sigmask, signals);
    }

protected:
    sigset_t _sigmask;
    struct timespec _timeout;
};
}}	// namespace nidas namespace util

#endif // NIDAS_UTIL_IOPOLLERABS_H
