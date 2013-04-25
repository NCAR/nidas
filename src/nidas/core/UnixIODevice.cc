/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/UnixIODevice.h>

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

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(_fd, &fdset);

    struct timespec tmpto = { msecTimeout / MSECS_PER_SEC,
        (msecTimeout % MSECS_PER_SEC) * NSECS_PER_MSEC };

    int res;
    if ((res = ::pselect(_fd+1,&fdset,0,0,&tmpto,&sigmask)) < 0) {
        throw nidas::util::IOException(getName(),"read",errno);
    }
    if (res == 0)
        throw nidas::util::IOTimeoutException(getName(),"read");
    return read(buf,len);
}
