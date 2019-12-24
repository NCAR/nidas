/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
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

#include <nidas/Config.h>   // HAVE_PPOLL

#include "UnixIODevice.h"

#ifdef HAVE_PPOLL
#define POLLING_METHOD POLL_POLL
#include <nidas/util/IoPoller.h>
#include <poll.h>
#else
#include <sys/select.h>
#endif

#include <signal.h>

using namespace nidas::core;
using namespace nidas::util;
using namespace std;

namespace n_u = nidas::util;

size_t UnixIODevice::read(void *buf, size_t len, int msecTimeout) throw(nidas::util::IOException)
{
    VLOG(("UnixIODevice::read(w/timeout): _fd = ") << _fd);

    IoPoller poller;
    poller.addSignals(SIG_BLOCK, SIGUSR1);
    poller.addPollee(_fd, POLLIN|POLLRDHUP);
    poller.changeTimeout(msecTimeout);
    int res = poller.poll();

    if (res < 0)
        throw nidas::util::IOException(getName(),"read(ppoll)",errno);
    if (res == 0)
        throw nidas::util::IOTimeoutException(getName(),"read(ppoll)");

    // assume only one device is being polled for this case, 
    // and its idx better be 0.
    int events = poller.getNextPolleeEvents(0).events;
    if (events & POLLERR)
        throw nidas::util::IOException(getName(),"read(ppoll)",errno);

    if (events & (POLLHUP | POLLRDHUP)) return 0;

    return read(buf,len);
}
