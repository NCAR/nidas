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

class RemoteSerialListener: public nidas::util::ServerSocket {
public:

    RemoteSerialListener(unsigned short port) throw(nidas::util::IOException);
    ~RemoteSerialListener() throw(nidas::util::IOException);

    const std::string getName() const
    {
        return getLocalSocketAddress().toString();
    }

    RemoteSerialConnection* acceptConnection() throw(nidas::util::IOException);

protected:
};

}}	// namespace nidas namespace core

#endif
