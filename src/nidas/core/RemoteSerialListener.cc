// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/RemoteSerialListener.h>
#include <nidas/core/SensorHandler.h>

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstdio>
#include <sys/epoll.h>

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
#if POLLING_METHOD == POLL_EPOLL_ET
        if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_DEL,_socket.getFd(),NULL) < 0) {
            n_u::IOException e("RemoteSerialListener","EPOLL_CTL_DEL",errno);
            _socket.close();
            throw e;
        }
#endif
        _socket.close();
    }
}

#if POLLING_METHOD == POLL_EPOLL_ET
bool
#else
void
#endif
RemoteSerialListener::handlePollEvents(uint32_t events) throw()
{
#if POLLING_METHOD == POLL_EPOLL_ET
    bool exhausted = false;
#endif

    if (events & N_POLLIN) {
        try {
            n_u::Socket* newsock = _socket.accept();
            RemoteSerialConnection *rsconn = new RemoteSerialConnection(newsock,_handler);
            _handler->scheduleAdd(rsconn);
#if POLLING_METHOD == POLL_EPOLL_ET
            exhausted = true;
#endif
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

#if POLLING_METHOD == POLL_EPOLL_ET
    return exhausted;
#endif
}

