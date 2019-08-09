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

#ifndef NIDAS_UTIL_PPOLL_H
#define NIDAS_UTIL_PPOLL_H

#include "IoPollerAbs.h"

#include <vector>

namespace nidas { namespace util {

/**
 * Interface for objects with a file descriptor, providing
 * a virtual method to be called when system calls such
 * as select, poll, or epoll indicate an event is pending
 * on the file descriptor.
 */
class PPoll : public IoPollerAbs {
public:
    PPoll() : IoPollerAbs(), _vFds(), _pFds(NULL), _nfds(0) {}
    PPoll(int msecDelay) : IoPollerAbs(msecDelay), _vFds(), _pFds(NULL), _nfds(0) {}
    virtual ~PPoll() 
    {
        _vFds.clear();
        delete [] _pFds;
    }

    virtual int poll();
    virtual void addPollee(int fd, int events);
    virtual void removePollee(int fd);
    virtual const event_data getNextPolleeEvents(uint64_t curIdx);

private:
    // vector for keeping track of pollfd structs. Used to update _pFds.
    std::vector<pollfd> _vFds;
    // array for passing pollfd structs into polling methods
    struct pollfd* _pFds;
    int _nfds;

    // do not copy
    PPoll(const PPoll&);
    PPoll& operator=(const PPoll&);
};

}}	// namespace nidas namespace util

#endif // NIDAS_UTIL_PPOLL_H
