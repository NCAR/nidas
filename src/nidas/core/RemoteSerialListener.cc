// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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

#include "RemoteSerialListener.h"
#include "SensorHandler.h"

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstdio>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

/**
 * Open a ServerSocket for listening on a given port.
 */
RemoteSerialListener::RemoteSerialListener(unsigned short port,
        SensorHandler* handler) throw(n_u::IOException):
	_socket(port),_handler(handler)
{
#if POLLING_METHOD == POLL_EPOLL_ET
    if (::fcntl(_socket.getFd(),F_SETFL,O_NONBLOCK) < 0)
        throw n_u::IOException("RemoteSerialListener","fcntl O_NONBLOCK",errno);
#endif

#if POLLING_METHOD == POLL_EPOLL_ET || POLLING_METHOD == POLL_EPOLL_LT
    epoll_event event = epoll_event();
#if POLLING_METHOD == POLL_EPOLL_ET
    event.events = EPOLLIN | EPOLLET;
#else
    event.events = EPOLLIN;
#endif
    event.data.ptr = this;

    if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_ADD,_socket.getFd(),&event) < 0)
        throw n_u::IOException("RemoteSerialListener","EPOLL_CTL_ADD",errno);
#endif
}

RemoteSerialListener::~RemoteSerialListener()
{
    try {
        close();
    }
    catch(const n_u::IOException & e) {
        PLOG(("%s", e.what()));
    }
}

void RemoteSerialListener::close() throw (n_u::IOException)
{
    if (_socket.getFd() >= 0) {
#if POLLING_METHOD == POLL_EPOLL_ET || POLLING_METHOD == POLL_EPOLL_LT
        if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_DEL,_socket.getFd(),NULL) < 0) {
            n_u::IOException e("RemoteSerialListener","EPOLL_CTL_DEL",errno);
            _socket.close();
            throw e;
        }
#endif
        _socket.close();
    }
}

bool
RemoteSerialListener::handlePollEvents(uint32_t events) throw()
{
    if (events & N_POLLIN) {
        try {
            n_u::Socket* newsock = _socket.accept();
            RemoteSerialConnection *rsconn = new RemoteSerialConnection(newsock,_handler);
            _handler->scheduleAdd(rsconn);
        }
        catch(const n_u::IOException & ioe) {
            PLOG(("RemoteSerialListener accept: %s", ioe.what()));
        }
    }
    if (events & N_POLLRDHUP) {
        PLOG(("RemoteSerialListener POLLRDHUP"));
    }
    if (events & (N_POLLERR | N_POLLHUP)) {
        PLOG(("RemoteSerialListener: POLLERR or POLLHUP"));
    }

    return true;
}

