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


