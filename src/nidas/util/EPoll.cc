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

#include "EPoll.h"
#include "IOTimeoutException.h"

#include <sys/epoll.h>

namespace nidas { namespace util {

int EPoll::poll()
{
    int nfdEvents = 0; //::ppoll(_pFds, _nfds, &_timeout, &_sigmask);

    // if (nfdEvents < 0)
    //     throw nidas::util::IOException(""/*getName()*/,"EPoll::poll()",errno);
    // if (nfdEvents == 0)
    //     throw nidas::util::IOTimeoutException(""/*getName()*/,"EPoll::poll()");

    return nfdEvents;
}

void EPoll::addPollee(int fd, int events)
{
    // struct pollfd pfd = {fd, (short int)events, 0};
    // _vFds.push_back(pfd);
    // _nfds = _vFds.size();

    // delete [] _pFds;
    // _pFds = new struct pollfd[_vFds.size()];
    // for (uint64_t i=0; i<_vFds.size(); ++i) {
    //     _pFds[i] = _vFds[i];
    // }
}

void EPoll::removePollee(int fd)
{
    // for (std::vector<struct pollfd>::iterator iter = _vFds.begin(); 
    //      iter != _vFds.end(); iter++) {
    //     if (iter->fd == fd) {
    //         _vFds.erase(iter);
    //         _nfds = _vFds.size();
    //         break;
    //     } 
    // }

    // delete [] _pFds;
    // _pFds = new struct pollfd[_vFds.size()];
    // for (uint64_t i=0; i<_vFds.size(); ++i) {
    //     _pFds[i] = _vFds[i];
    // }
}

const event_data EPoll::getNextPolleeEvents(uint64_t curIdx)
{
    event_data eventData;
    // for (int i=curIdx; i<_nfds; ++i) {
    //     if (_pFds[i].revents) {
    //         eventData.fd = _pFds[i].fd;
    //         eventData.idx = i;
    //         eventData.events = _pFds[i].revents;
    //         break;
    //     }
    // }

    return eventData;
}

}}	// namespace nidas namespace core
