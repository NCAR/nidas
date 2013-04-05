// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_REMOTESERIALLISTENER_H
#define NIDAS_CORE_REMOTESERIALLISTENER_H

#include <nidas/util/IOException.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Socket.h>
#include <nidas/core/SampleClient.h>
#include <nidas/core/RemoteSerialConnection.h>
#include <nidas/core/EpollFd.h>

namespace nidas { namespace core {

class SensorHandler;

class RemoteSerialListener: public EpollFd
{
public:

    RemoteSerialListener(unsigned short port, SensorHandler*)
        throw(nidas::util::IOException);

    ~RemoteSerialListener();

    void handleEpollEvents(uint32_t events) throw();

    void close() throw(nidas::util::IOException);

    const std::string getName() const
    {
        return _socket.getLocalSocketAddress().toString();
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
