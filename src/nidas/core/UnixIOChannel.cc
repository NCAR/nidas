// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2012-08-01 14:58:11 -0600 (Wed, 01 Aug 2012) $

    $LastChangedRevision: 6565 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/UnixIOChannel.cc $
 ********************************************************************
*/

#include "UnixIOChannel.h"

using namespace nidas::core;

void UnixIOChannel::setNonBlocking(bool val) throw (nidas::util::IOException)
{
    if (_fd >= 0) {
        int flags;
        if ((flags = ::fcntl(_fd,F_GETFL,0)) < 0)
            throw nidas::util::IOException(getName(),"fcntl(,F_GETFL,)",errno);
        int nflags;
        if (val) nflags = flags | O_NONBLOCK;
        else nflags = flags & ~O_NONBLOCK;
        if (nflags != flags && ::fcntl(_fd,F_SETFL,0,nflags) < 0)
            throw nidas::util::IOException(getName(),"fcntl(,F_SETFL,)",errno);
    }
}

/**
 * Do fcntl to determine value of O_NONBLOCK flag.
 */
bool UnixIOChannel::isNonBlocking() const throw (nidas::util::IOException)
{
    if (_fd >= 0) {
        int flags;
        if ((flags = ::fcntl(_fd,F_GETFL,0)) < 0)
            throw nidas::util::IOException(getName(),"fcntl(,F_GETFL,)",errno);
        return (flags & O_NONBLOCK) != 0;
    }
    return false;
}


