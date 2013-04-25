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

class RemoteSerialListener: public Polled
{
public:

    RemoteSerialListener(unsigned short port, SensorHandler*)
        throw(nidas::util::IOException);

    ~RemoteSerialListener();

#if POLLING_METHOD == POLL_EPOLL_ET
    bool handlePollEvents(uint32_t events) throw();
#else
    void handlePollEvents(uint32_t events) throw();
#endif

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
