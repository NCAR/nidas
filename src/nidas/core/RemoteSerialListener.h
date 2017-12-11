// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_REMOTESERIALLISTENER_H
#define NIDAS_CORE_REMOTESERIALLISTENER_H

#include <nidas/util/IOException.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Socket.h>
#include "SampleClient.h"
#include "RemoteSerialConnection.h"
#include "Polled.h"

namespace nidas { namespace core {

class SensorHandler;

class RemoteSerialListener: public Polled
{
public:

    RemoteSerialListener(unsigned short port, SensorHandler*)
        throw(nidas::util::IOException);

    ~RemoteSerialListener();

    bool handlePollEvents(uint32_t events) throw();

    void close() throw(nidas::util::IOException);

    int getFd() const { return _socket.getFd(); }

    const std::string getName() const
    {
        return _socket.getLocalSocketAddress().toAddressString();
    }

private:

    nidas::util::ServerSocket _socket;

    SensorHandler* _handler;

    // no copying
    RemoteSerialListener(const RemoteSerialListener&);

    // no assignment
    RemoteSerialListener& operator=(const RemoteSerialListener&);
};

}}	// namespace nidas namespace core

#endif
