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

#include <OutputStream.h>
#include <atdUtil/Socket.h>

#include <string>
#include <iostream>

namespace dsm {

/**
 * Implementation of an OutputStream over an atdUtil::Socket.
 */
class SocketOutputStream: public OutputStream {

public:
  SocketOutputStream(atdUtil::Socket& socket) :
  	OutputStream(socket.getSendBufferSize()),sock(socket) {}

  void close() throw(atdUtil::IOException) {
      sock.close();
  }
  /**
   * Do the actual hardware write.
   */
  size_t devWrite(const void* buf, size_t len) throw (atdUtil::IOException)
  {
      std::cerr << "SocketOutputStream::devWrite, len=" << len << std::endl;
      return sock.send(buf,len);
  }

protected:
  atdUtil::Socket sock;
};

}

#endif
