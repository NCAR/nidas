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

#ifndef NIDAS_UTIL_IOPOLLER_H
#define NIDAS_UTIL_IOPOLLER_H

#include "IoPollerIf.h"

#include <cassert>

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
#if !defined(POLLING_METHOD)
    #define POLLING_METHOD POLL_PSELECT
#endif

namespace nidas { namespace util {

/**
 * Facade for the IoPollerIf subclassed object which polls other objects which are 
 * accessed with a file descriptor.
 */
class IoPoller : public IoPollerIf 
{
public:
    IoPoller();
    virtual ~IoPoller()
    {
        delete _pIoPollerImpl;
    }

    // method which actually waits on the pollees
    virtual int poll() {assert(_pIoPollerImpl); return _pIoPollerImpl->poll();}

    // add/remove items to be included in the poll
    virtual void addPollee(int fd, int events) {assert(_pIoPollerImpl); _pIoPollerImpl->addPollee(fd, events);}
    virtual void removePollee(int fd) {assert(_pIoPollerImpl); _pIoPollerImpl->removePollee(fd);}

    // adjust timeout
    virtual void changeTimeout(int msecTimeout) {assert(_pIoPollerImpl); _pIoPollerImpl->changeTimeout(msecTimeout);}

    // add/remove signals
    virtual void addSignals(unsigned long how, unsigned long int signals) {assert(_pIoPollerImpl); _pIoPollerImpl->addSignals(how, signals);}
    virtual void removeSignals(unsigned long int how, unsigned long int signals) {assert(_pIoPollerImpl); _pIoPollerImpl->removeSignals(how, signals);}

    // Get the events from next pollee which has events pending
    virtual const event_data getNextPolleeEvents(uint64_t curIdx) {assert(_pIoPollerImpl); _pIoPollerImpl->getNextPolleeEvents(currIdx);}

private:
    IoPollerIf* _pIoPollerImpl;
};
}}	// namespace nidas namespace util

#endif // NIDAS_UTIL_IOPOLLER_H
