/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_REMOTESERIALLISTENER_H
#define DSM_REMOTESERIALLISTENER_H

#include <atdUtil/IOException.h>
#include <atdUtil/EOFException.h>
#include <SampleClient.h>
#include <RemoteSerialConnection.h>

namespace dsm {

class RemoteSerialListener {
public:


    RemoteSerialListener() throw(atdUtil::IOException);
    ~RemoteSerialListener() throw(atdUtil::IOException);

    const std::string& getName() const { return name; }

    int getListenSocketFd() const { return socketfd; }
    RemoteSerialConnection* acceptConnection() throw(atdUtil::IOException);

protected:
    const int RSERIAL_PORT;
    int socketfd;		// listen socket fd
    std::string name;
};

}
#endif
