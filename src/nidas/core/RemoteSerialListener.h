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

namespace nidas { namespace core {

class RemoteSerialListener {
public:

    RemoteSerialListener(unsigned short port) throw(nidas::util::IOException);

    ~RemoteSerialListener() throw(nidas::util::IOException);

    const std::string getName() const
    {
        return _socket.getLocalSocketAddress().toString();
    }

    RemoteSerialConnection* acceptConnection() throw(nidas::util::IOException);

    int getFd() const
    {
        return _socket.getFd();
    }

private:

    nidas::util::ServerSocket _socket;
};

}}	// namespace nidas namespace core

#endif
