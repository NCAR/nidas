/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_SOCKETOUTPUTSTREAM_H
#define DSM_SOCKETOUTPUTSTREAM_H

#include <string>

#include <OutputStream.h>
#include <atdUtil/Socket.h>

namespace dsm {

/**
 * Implementation of an OutputStream over an atdUtil::Socket.
 */
class SocketOutputStream: public OutputStream {

public:
  SocketOutputStream(atdUtil::Socket& socket) :
  	OutputStream(sock.getSendBufferSize()),sock(socket) {}

  /**
   * Do the actual hardware write.
   */
  size_t devWrite(const void* buf, size_t len) throw (atdUtil::IOException)
  {
      return sock.send(buf,len);
  }

protected:
  atdUtil::Socket sock;
};

}

#endif
