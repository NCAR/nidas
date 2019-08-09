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

#ifndef NIDAS_UTIL_SPOLL_H
#define NIDAS_UTIL_SPOLL_H

#include "IoPollerAbs.h"

#include <sys/select.h>
#include <cstdio>
#include <vector>

namespace nidas { namespace util {

/**
 * Interface for objects with a file descriptor, providing
 * a virtual method to be called when system calls such
 * as select, poll, or epoll indicate an event is pending
 * on the file descriptor.
 */
class SPoll : public IoPollerAbs {
public:
    typedef std::vector<int> fdvector;
    SPoll() : IoPollerAbs(), _vFds(), _rFdSet(), _eFdSet(), 
                             _working_rFdSet(), _working_eFdSet(), _sfd(0) 
    {
        FD_ZERO(&_rFdSet);
        FD_ZERO(&_eFdSet);
    }

    SPoll(int msecTimeout) : IoPollerAbs(msecTimeout), _vFds(), _rFdSet(), _eFdSet(), 
                                                       _working_rFdSet(), _working_eFdSet(), _sfd(0) 
    {
        FD_ZERO(&_rFdSet);
        FD_ZERO(&_eFdSet);
    }

    virtual ~SPoll() 
    {
        _vFds.clear();
        FD_ZERO(&_rFdSet);
        FD_ZERO(&_eFdSet);
    }

    virtual int poll();
    virtual void addPollee(int fd, int events);
    virtual void removePollee(int fd);
    virtual const event_data getNextPolleeEvents(uint64_t curIdx);

private:
    // vector for keeping track of pollfd structs. Used to update _pFds.
    fdvector _vFds;
    // bit arrays for holding file descriptors to use in select
    fd_set _rFdSet;
    fd_set _eFdSet;
    // actual bit arrays for file descriptors to use in select
    // copied from _rFdSet, _eFdSet when calling select
    fd_set _working_rFdSet;
    fd_set _working_eFdSet;

    // highest fd+1 of added pollees used in select
    int _sfd;

    // do not copy
    SPoll(const SPoll&);
    SPoll& operator=(const SPoll&);
};

}}	// namespace nidas namespace util

#endif // NIDAS_UTIL_SPOLL_H
