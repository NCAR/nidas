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

#ifndef NIDAS_UTIL_IOPOLLERIF_H
#define NIDAS_UTIL_IOPOLLERIF_H

#include <sys/poll.h>
#include <stdint.h>

/**
 * epoll.h defines EPOLLIN, EPOLLERR, EPOLLHUP, EPOLLRDHUP
 * poll.h defines POLLIN, POLLERR, POLLHUP, POLLRDHUP.
 * As of glibc 2.12 and 2.16 they have equal values.
 *
 * Define local macros N_POLLIN, N_POLLERR, N_POLLHUP, N_POLLRDHUP 
 * from the poll.h values, so that code compiles with which ever
 * system header file is used.
 */

#undef N_POLLIN
#define N_POLLIN POLLIN
#undef N_POLLERR
#define N_POLLERR POLLERR
#undef N_POLLHUP
#define N_POLLHUP POLLHUP

// POLLRDHUP is somewhat new (Linux 2.6.17)
// We're already @ Linux 4.9 on RPi DSMs
#undef N_POLLRDHUP
#ifdef POLLRDHUP
#define N_POLLRDHUP POLLRDHUP
#else
#define N_POLLRDHUP POLLHUP
#endif

namespace nidas { namespace util {

struct event_data {
    int fd;
    int idx;
    int events;
    event_data() : fd(-1), idx(-1), events(0) {}
    bool isValid() {return (fd >= 0 && idx >= 0);}
};

/**
 * Interface for objects with a file descriptor, providing
 * virtual methods to add/remove/poll for such system polling
 * methods as select, poll, or epoll.
 */
class IoPollerIf {
public:
    virtual ~IoPollerIf() {}

    // method which actually waits on the pollees
    virtual int poll() = 0;

    // add/remove items to be included in the poll
    virtual void addPollee(int fd, int events) = 0;
    virtual void removePollee(int fd) = 0;

    // adjust timeout
    virtual void changeTimeout(int msecTimeout) = 0;

    // add/remove signals
    virtual void addSignals(unsigned long how, unsigned long int signals) = 0;
    virtual void removeSignals(unsigned long int how, unsigned long int signals) = 0;

    // Get the events from next pollee which has events pending
    virtual const event_data getNextPolleeEvents(uint64_t curIdx) = 0;
};
}}	// namespace nidas namespace util

#endif // NIDAS_UTIL_IOPOLLERIF_H
