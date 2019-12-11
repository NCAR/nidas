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

#include "IoPoller.h"
#include "EPoll.h"
#include "PPoll.h"
#include "SPoll.h"

namespace nidas { namespace util {

IoPoller::IoPoller() 
    : IoPollerIf(), _pIoPollerImpl(0)
{
    #if POLLING_METHOD == POLL_EPOLL
        #error "EPoll is not yet implemented!!"
        #if !defined(HAVE_EPOLL_PWAIT)
            #error "Defined polling method is epoll, but system doesn't support it!"
        #endif
    _pIoPollerImpl = new EPoll;
    assert(_pIoPollerImpl);

    #elif POLLING_METHOD == POLL_POLL
        #if !defined(HAVE_PPOLL)
            #error "Defined polling method is ppoll, but system doesn't support it!"
        #endif
    _pIoPollerImpl = new PPoll;
    assert(_pIoPollerImpl);
    
    #elif POLLING_METHOD == POLL_PSELECT
    _pIoPollerImpl = new SPoll;
    assert(_pIoPollerImpl);
    
    #else
        #error "No available io device polling methods found!!"
    #endif
}

}}	// namespace nidas namespace util
