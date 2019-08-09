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

#include "SPoll.h"
#include "IOTimeoutException.h"
#include "time_constants.h"

#include<sys/time.h>
#include <algorithm>

namespace nidas { namespace util {

int SPoll::poll()
{
    timeval timeout;
    timeout.tv_sec = _timeout.tv_sec;
    timeout.tv_usec = _timeout.tv_nsec/NSECS_PER_USEC;
    _working_rFdSet = _rFdSet;
    _working_eFdSet = _eFdSet;
    int nfds = ::select(_sfd, &_working_rFdSet, 0, &_working_eFdSet, &timeout);

    if (nfds < 0)
        throw nidas::util::IOException(""/*getName()*/,"SPoll::poll()",errno);
    if (nfds == 0)
        throw nidas::util::IOTimeoutException(""/*getName()*/,"SPoll::poll()");

    return nfds;
}

void SPoll::addPollee(int fd, int /*events*/)
{
    // check to see if it's already in the vector...
    uint64_t idx = 0;
    if (!_vFds.empty()) {
        DLOG(("SPoll::addPollee(): current set of filedes is not empty. See if fd is already in there."));
        for (fdvector::iterator it=_vFds.begin(); idx<_vFds.size(); ++idx, ++it) {
            if (_vFds[idx] == fd) {
                // already in the vector, bug out...
                return;
            }
            // insert it so that the vector is always sorted increasing.
            else if (_vFds[idx] > fd) {
                _vFds.insert(it, fd);
                break;
            }
        }
    }
    else if (!idx || idx == _vFds.size()) {
        _vFds.push_back(fd);
    }

    // can't hurt to set it twice...
    FD_SET(fd, &_rFdSet);
    FD_SET(fd, &_eFdSet);

    _sfd = std::max(_sfd, fd+1);
}

void SPoll::removePollee(int fd)
{
    for (std::vector<int>::iterator iter = _vFds.begin(); 
         iter != _vFds.end(); 
         iter++) {
        if (*iter == fd) {
            _vFds.erase(iter);
            break;
        } 
        // this is always the one before. 
        // So it's valid regardless whether iter was the last item
        // or an intermediate item...
        _sfd = std::max(_sfd, *iter+1);
    }

    // clear it regardless whether it's in the list
    FD_CLR(fd, &_rFdSet);
    FD_CLR(fd, &_eFdSet);
}

const event_data SPoll::getNextPolleeEvents(uint64_t curIdx)
{
    event_data eventData;
    if (curIdx < _vFds.size()) {
        for (uint64_t i=curIdx; i<_vFds.size(); ++i) {
            if (FD_ISSET(_vFds[curIdx], &_working_rFdSet)) {
                eventData.fd = _vFds[i];
                eventData.idx = i;
                eventData.events |= N_POLLIN;
                if (FD_ISSET(_vFds[curIdx], &_working_eFdSet)) {
                    eventData.events |= N_POLLERR;
                }
            }
        }
    }

    return eventData;
}


}}	// namespace nidas namespace core
