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

#ifndef NIDAS_CORE_POLLED_H
#define NIDAS_CORE_POLLED_H

/**
 * Enumeration of file descriptor polling methods supported by SensorHander.
 */
#define POLL_EPOLL_ET   0       /* epoll edge-triggered */
#define POLL_EPOLL_LT   1       /* epoll level-triggered */
#define POLL_PSELECT    2       /* pselect */
#define POLL_POLL       3       /* poll/ppoll */

/**
 * Select a POLLING_METHOD. Also see discussion in SensorHandler.
 *
 * The EPOLL methods are the most efficient, since the OS can cache
 * the file descriptors of interest. They are not passed on each poll.
 * The system is only notified when they change.
 *
 * POLL_EPOLL_ET (edge-triggered) may also be a bit more efficient
 * than POLL_EPOLL_LT (level-triggered), but there is one
 * circumstance where it doesn't work in NIDAS.
 *
 * Edge-triggering will only return a EPOLLIN event on a
 * transition from no-data-available to data-available, so on
 * an EPOLLIN, one must read all data available from a descriptor,
 * using non-blocking IO, in order for an EPOLLIN event to occur again.
 *
 * Read methods in NIDAS return a length of 0 on EAGAIN/EWOULDBLOCK,
 * rather than an exception. So when using edge-triggering, one
 * must do non-blocking reads on a descriptor until a return of 0.
 * However with UDP sockets, incoming packets can have an actual
 * length of 0 (we saw this on a NovAtel GPS), and so a read return
 * of zero doesn't necesarily indicate there is no data left to read.
 * Due to this bug, on receipt of a zero-length UDP packet using
 * edge-triggering, NIDAS will cease to get EPOLLIN events on that socket.
 *
 * Fixing this would require changing NIDAS reads to return
 * an exception, or perhaps -1, on EAGAIN/EWOULDBLOCK. The possible
 * gains of edge-triggering do not seem to be worth it, so we'll
 * use level-triggering.
 */
#define POLLING_METHOD POLL_EPOLL_LT

#include <sys/poll.h>

/**
 * epoll.h defines EPOLLIN, EPOLLERR, EPOLLHUP, EPOLLRDHUP
 * poll.h defines POLLIN, POLLERR, POLLHUP, POLLRDHUP.
 * As of glibc 2.12 and 2.16 they have equal values.
 *
 * Define local macros N_POLLIN, N_POLLERR, N_POLLHUP, N_POLLRDHUP 
 * from the poll.h values, so that code compiles with which ever
 * system header file is used.
 */

#define N_POLLIN POLLIN
#define N_POLLERR POLLERR
#define N_POLLHUP POLLHUP

// POLLRDHUP is somewhat new (Linux 2.6.17)
#ifdef POLLRDHUP
#define N_POLLRDHUP POLLRDHUP
#else
#define N_POLLRDHUP POLLHUP
#endif

#if POLLING_METHOD == POLL_PSELECT
#include <sys/select.h>
#endif

#if POLLING_METHOD == POLL_EPOLL_ET || POLLING_METHOD == POLL_EPOLL_LT
#include <sys/epoll.h>
#endif

namespace nidas { namespace core {

/**
 * Interface for objects with a file descriptor, providing
 * a virtual method to be called when system calls such
 * as select, poll, or epoll indicate an event is pending
 * on the file descriptor.
 */
class Polled {
public:
    virtual ~Polled() {}

    virtual int getFd() const = 0;

    /**
     * @return: true: read consumed all available data, false otherwise.
     * This return value is required for edge-triggered polling
     * with epoll, since a read event won't be re-triggered on
     * a file descriptor until all available data is read.
     */
    virtual bool handlePollEvents(uint32_t events) throw() = 0;
};

}}	// namespace nidas namespace core

#endif
