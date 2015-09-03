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

#include <nidas/core/UnixIODevice.h>

#ifdef HAVE_PPOLL
#include <poll.h>
#else
#include <sys/select.h>
#endif

#include <signal.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

size_t UnixIODevice::read(void *buf, size_t len, int msecTimeout) throw(nidas::util::IOException)
{
    // If the user blocks SIGUSR1 prior to calling readBuffer,
    // then we can catch it here in the pselect.
    sigset_t sigmask;
    pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
    // unblock SIGUSR1 in ppoll/pselect
    sigdelset(&sigmask,SIGUSR1);

#ifdef HAVE_PPOLL
    struct pollfd fds;
    fds.fd =  _fd;
#ifdef POLLRDHUP
    fds.events = POLLIN | POLLRDHUP;
#else
    fds.events = POLLIN;
#endif
#else
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(_fd, &fdset);
#endif

    struct timespec tmpto = { msecTimeout / MSECS_PER_SEC,
        (msecTimeout % MSECS_PER_SEC) * NSECS_PER_MSEC };

    int res;

#ifdef HAVE_PPOLL
    if ((res = ::ppoll(&fds,1,&tmpto,&sigmask)) < 0)
        throw nidas::util::IOException(getName(),"read",errno);
    if (res == 0)
        throw nidas::util::IOTimeoutException(getName(),"read");

    if (fds.revents & POLLERR)
        throw nidas::util::IOException(getName(),"read",errno);

#ifdef POLLRDHUP
    if (fds.revents & (POLLHUP | POLLRDHUP)) return 0;
#else
    if (fds.revents & POLLHUP) return 0;
#endif
#else
    if ((res = ::pselect(_fd+1,&fdset,0,0,&tmpto,&sigmask)) < 0) {
        throw nidas::util::IOException(getName(),"read",errno);
    }
    if (res == 0)
        throw nidas::util::IOTimeoutException(getName(),"read");
#endif
    return read(buf,len);
}
