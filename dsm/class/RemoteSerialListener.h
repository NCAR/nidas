/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_REMOTESERIALLISTENER_H
#define DSM_REMOTESERIALLISTENER_H

#include <atdUtil/IOException.h>
#include <atdUtil/EOFException.h>
#include <atdUtil/Socket.h>
#include <SampleClient.h>
#include <RemoteSerialConnection.h>

namespace dsm {

class RemoteSerialListener: public atdUtil::ServerSocket {
public:

    RemoteSerialListener() throw(atdUtil::IOException);
    ~RemoteSerialListener() throw(atdUtil::IOException);

    const std::string getName() const
    {
        return getInet4SocketAddress().toString();
    }

    RemoteSerialConnection* acceptConnection() throw(atdUtil::IOException);

protected:
};

}
#endif
