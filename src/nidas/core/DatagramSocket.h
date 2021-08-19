// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_DATAGRAMSOCKET_H
#define NIDAS_CORE_DATAGRAMSOCKET_H

#include "IOChannel.h"
#include "DOMable.h"
#include <nidas/util/Socket.h>
#include <nidas/util/auto_ptr.h>

#include <string>
#include <iostream>

namespace nidas { namespace core {

/**
 * Implementation of an IOChannel, over a DatagramSocket.
 *
 */
class DatagramSocket: public IOChannel {

public:

    /**
     * Constructor.
     */
    DatagramSocket();

    /**
     * Copy constructor.
     */
    DatagramSocket(const DatagramSocket& x);

    DatagramSocket& operator=(const DatagramSocket& rhs);

    DatagramSocket(nidas::util::DatagramSocket* sock);

    ~DatagramSocket();

    DatagramSocket* clone() const;

    /**
     * @throws nidas::util::IOException
     **/
    void requestConnection(IOChannelRequester* service);

    /**
     * @throws nidas::util::IOException
     **/
    IOChannel* connect();

    bool isNewInput() const { return false; }

    bool writeNidasHeader() const { return false; }

    /**
     * @throws nidas::util::IOException
     **/
    std::list<nidas::util::Inet4NetworkInterface> getInterfaces() const
    {
        if (_nusocket) return _nusocket->getInterfaces();
        return std::list<nidas::util::Inet4NetworkInterface>();
    }

    /**
     * Do setNonBlocking(val) on underlying socket.
     *
     * @throws nidas::util::IOException
     **/
    void setNonBlocking(bool val)
    {
        _nonBlocking = val;
        if (_nusocket) _nusocket->setNonBlocking(val);
    }

    /**
     * Return isNonBlocking() of underlying socket.
     *
     * @throws nidas::util::IOException
     **/
    bool isNonBlocking() const
    {
	if (_nusocket) return _nusocket->isNonBlocking();
	return _nonBlocking;
    }

    size_t getBufferSize() const throw();

    /**
     * Do the actual hardware read.
     *
     * @throws nidas::util::IOException
     **/
    size_t read(void* buf, size_t len)
    {
        return _nusocket->recv(buf,len);
    }

    /**
     * Do the actual hardware write.
     *
     * @throws nidas::util::IOException
     **/
    size_t write(const void* buf, size_t len)
    {
        return _nusocket->send(buf,len,0);
    }

    /**
     * Do the actual hardware write.
     *
     * @throws nidas::util::IOException
     **/
    size_t write(const struct iovec* iov, int iovcnt)
    {
        return _nusocket->send(iov,iovcnt,0);
    }

    /**
     * @throws nidas::util::IOException
     **/
    void close()
    {
        if (_nusocket) _nusocket->close();
    }


    int getFd() const
    {
        if (_nusocket) return _nusocket->getFd();
	return -1;
    }

    const std::string& getName() const { return _name; }

    void setName(const std::string& val) { _name = val; }

    /**
     *  Get the unix path name.
     */
    const std::string& getUnixPath() const
    {
        return _unixPath;
    }

    /**
     * Set the pathname for the unix socket connection.
     */
    void setUnixPath(const std::string& unixPath);

    /**
     * Set address for this socket. If address is INADDR_ANY
     * or an address of a local interface, then a bind
     * will be done to that address.
     * If the address is a remote host, then a connect
     * will be done, which sets the default sendto address.
     */
    void setSocketAddress(const nidas::util::SocketAddress& val);

    /**
     * @throws nidas::util::UnknownHostException
     **/
    const nidas::util::SocketAddress& getSocketAddress();

    /**
     * Set the hostname and port of the remote connection. This
     * method does not try to do a DNS host lookup. The DNS lookup
     * will be done at connect time.
     */
    void setHostPort(const std::string& host,unsigned short port);

    /**
     * Set the the local port number.
     */
    void setPort(unsigned short port);

    /**
     *  Get the name of the remote host.
     */
    const std::string& getHost() const
    {
        return _host;
    }

    unsigned short getPort() const
    {
        return _port;
    }

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement*);

private:

    nidas::util::auto_ptr<nidas::util::SocketAddress> _sockAddr;

    std::string _host;

    unsigned short _port;

    std::string _unixPath;

    nidas::util::DatagramSocket* _nusocket;

    std::string _name;

    IOChannelRequester* _iochanRequester;

    bool _nonBlocking;

};

}}	// namespace nidas namespace core

#endif
