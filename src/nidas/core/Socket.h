// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_SOCKET_H
#define NIDAS_CORE_SOCKET_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/DOMable.h>
#include <nidas/util/Socket.h>
#include <nidas/util/Thread.h>
#include <nidas/util/UTime.h>

#include <string>
#include <iostream>
#include <memory>  // auto_ptr<>

namespace nidas { namespace core {

/**
 * Implementation of an IOChannel, over a Socket.
 */
class Socket: public IOChannel {

public:

    /**
     * Constructor.
     */
    Socket();

    /**
     * Constructor from a connected nidas::util::Socket.
     * @param sock Pointer to the connected nidas::util::Socket.
     * Socket will own the pointer and will delete it
     * it its destructor.
     */
    Socket(nidas::util::Socket* sock);

    ~Socket();

    Socket* clone() const;

    void requestConnection(IOChannelRequester* service)
    	throw(nidas::util::IOException);

    IOChannel* connect()
        throw(nidas::util::IOException,nidas::util::UnknownHostException);

    virtual bool isNewInput() const { return _newInput; }

    /*
     * The requestType is set when a socket connection has been
     * established with McSocket.
     */
    void setRequestType(enum McSocketRequest val)
    {
        _requestType = val;
    }

    enum McSocketRequest getRequestType() const
    {
        return _requestType;
    }

    /**
     * Do setKeepAliveIdleSecs(int secs) on underlying socket.
     */
    void setKeepAliveIdleSecs(int val) throw (nidas::util::IOException)
    {
	_keepAliveIdleSecs = val;
	if (_nusocket) _nusocket->setKeepAliveIdleSecs(val);
    }

    /**
     * Return getKeepAliveIdleSecs() on underlying socket.
     */
    int getKeepAliveIdleSecs() const throw (nidas::util::IOException)
    {
	if (_nusocket) return _nusocket->getKeepAliveIdleSecs();
	return _keepAliveIdleSecs;
    }

    std::list<nidas::util::Inet4NetworkInterface> getInterfaces() const
        throw(nidas::util::IOException)
    {
        if (_nusocket) return _nusocket->getInterfaces();
        return std::list<nidas::util::Inet4NetworkInterface>();
    }

    /**
     * Do setNonBlocking(val) on underlying socket.
     */
    void setNonBlocking(bool val) throw (nidas::util::IOException)
    {
	_nonBlocking = val;
	if (_nusocket) _nusocket->setNonBlocking(val);
    }

    /**
     * Return isNonBlocking() of underlying socket.
     */
    bool isNonBlocking() const throw (nidas::util::IOException)
    {
	if (_nusocket) return _nusocket->isNonBlocking();
	return _nonBlocking;
    }

    size_t getBufferSize() const throw();

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (nidas::util::IOException)
    {
        if (_firstRead) _firstRead = false;
        else _newInput = false;
        return _nusocket->recv(buf,len);
    }

    /**
     * Do the actual hardware write.
     */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    {
	// std::cerr << "nidas::core::Socket::write, len=" << len << std::endl;
#ifdef DEBUG
	std::cerr << "writing, now=" << nidas::util::UTime().format(true,"%H%M%S.%3f") << " len=" << len << std::endl;
#endif
	return _nusocket->send(buf,len, MSG_NOSIGNAL);
    }

    /**
     * Do the actual hardware write.
     */
    size_t write(const struct iovec* iov, int iovcnt) throw (nidas::util::IOException)
    {
	// std::cerr << "nidas::core::Socket::write, len=" << len << std::endl;
#ifdef DEBUG
	size_t l = 0;
	for (int i =0; i < iovcnt; i++) l += iov[i].iov_len;
	std::cerr << "writing, len=" << l << std::endl;
#endif
	return _nusocket->send(iov,iovcnt, MSG_NOSIGNAL);
    }

    void close() throw (nidas::util::IOException);

    int getFd() const
    {
        if (_nusocket) return _nusocket->getFd();
	return -1;
    }

    const std::string& getName() const { return _name; }

    void setName(const std::string& val) { _name = val; }

    nidas::util::Inet4Address getRemoteInet4Address();

    /**
     * Set the hostname and port of the remote connection. This
     * method does not try to do a DNS host lookup. The DNS lookup
     * will be done at connect time.
     */
    void setRemoteHostPort(const std::string& host,unsigned short port);

    /**
     *  Get the name of the remote host.
     */
    const std::string& getRemoteHost() const
    {
        return _remoteHost;
    }

    unsigned short getRemotePort() const
    {
        return _remotePort;
    }

    /**
     *  Get the name of the remote host.
     */
    const std::string& getRemoteUnixPath() const
    {
        return _unixPath;
    }

    /**
     * Set the pathname for the unix socket connection.
     */
    void setRemoteUnixPath(const std::string& unixPath);

    void setRemoteSocketAddress(const nidas::util::SocketAddress& val);

    /**
     * This method does a DNS lookup of the value of getRemoteHost(),
     * and so it can throw an UnknownHostException. This is different
     * behaviour from the the nidas::util::Socket::getRemoteSocketAddress()
     * method, which does not throw an exception.
     */
    const nidas::util::SocketAddress& getRemoteSocketAddress()
        throw(nidas::util::UnknownHostException);

    /**
     * Create either a Socket or a McSocket from a DOMElement.
     */
    static IOChannel* createSocket(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

    class ConnectionThread: public nidas::util::Thread
    {
    public:
        ConnectionThread(Socket* sock);
        ~ConnectionThread();

        int run() throw(nidas::util::IOException);

        void interrupt();

    private:
        Socket* _socket;
        ConnectionThread(const ConnectionThread&);
        ConnectionThread& operator=(const ConnectionThread&);
    };

protected:
    /**
     * Copy constructor.
     */
    Socket(const Socket& x);

private:

    std::auto_ptr<nidas::util::SocketAddress> _remoteSockAddr;

    std::string _remoteHost;

    unsigned short _remotePort;

    std::string _unixPath;

    nidas::util::Socket* _nusocket;

    std::string _name;

    IOChannelRequester* _iochanRequester;

    ConnectionThread* _connectionThread;

    bool _firstRead;

    bool _newInput;

    int _keepAliveIdleSecs;

    bool _nonBlocking;

    nidas::util::Mutex _connectionMutex;

    enum McSocketRequest _requestType;

    /**
     * No assignment.
     */
    Socket& operator=(const Socket&);
};

/**
 * Implementation of an IOChannel, over a ServerSocket.
 */
class ServerSocket: public IOChannel {

public:

    /**
     * Constructor.
     */
    ServerSocket();

    /**
     * Constructor.
     */
    ServerSocket(const nidas::util::SocketAddress& addr);

    ~ServerSocket();

    ServerSocket* clone() const;

    void requestConnection(IOChannelRequester* service)
    	throw(nidas::util::IOException);

    IOChannel* connect()
        throw(nidas::util::IOException);

    const std::string& getName() const { return _name; }

    void setName(const std::string& val) { _name = val; }

    int getFd() const
    {
        if (_servSock) return _servSock->getFd();
	return -1;
    }

    /**
     * Set the value of keepAliveIdleSecs.  This is set on each
     * accepted socket connection. It does not pertain to the socket
     * which is waiting for connections.
     */
    void setKeepAliveIdleSecs(int val) throw (nidas::util::IOException)
    {
	_keepAliveIdleSecs = val;
    }

    /**
     * Return keepAliveIdleSecs for this ServerSocket.
     */
    int getKeepAliveIdleSecs() const throw (nidas::util::IOException)
    {
	return _keepAliveIdleSecs;
    }

    /**
     * The blocking flag that will be set on accepted connections.
     * The ServerSocket itself is always non-blocking.
     */
    void setNonBlocking(bool val) throw (nidas::util::IOException)
    {
	_nonBlocking = val;
    }

    /**
     * 
     */
    bool isNonBlocking() const throw (nidas::util::IOException)
    {
	return _nonBlocking;
    }

    /**
    * ServerSocket will never be called to do an actual read.
    */
    size_t read(void*, size_t) throw (nidas::util::IOException)
    {
	assert(false);
	return 0;
    }

    /**
    * ServerSocket should never be called to do an actual write.
    */
    size_t write(const void*, size_t) throw (nidas::util::IOException)
    {
	assert(false);
	return 0;
    }

    /**
     * ServerSocket should never be called to do an actual write.
     */
    size_t write(const struct iovec*, int) throw (nidas::util::IOException)
    {
	assert(false);
	return 0;
    }

    void close() throw (nidas::util::IOException);

    /**
     * Create either a Socket or a McSocket from a DOMElement.
     */
    static IOChannel* createSocket(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement*
        toDOMParent(xercesc::DOMElement* parent)
                throw(xercesc::DOMException);

    xercesc::DOMElement*
        toDOMElement(xercesc::DOMElement* node)
                throw(xercesc::DOMException);

    class ConnectionThread: public nidas::util::Thread
    {
    public:
        ConnectionThread(ServerSocket* sock);
        int run() throw(nidas::util::IOException);
        void interrupt();

    private:
        ServerSocket* _socket;
        ConnectionThread(const ConnectionThread&);
        ConnectionThread& operator=(const ConnectionThread&);
    };

protected:
    /**
     * Copy constructor.
     */
    ServerSocket(const ServerSocket& x);

private:

    std::auto_ptr<nidas::util::SocketAddress> _localSockAddr;

    std::string _name;

    nidas::util::ServerSocket* _servSock;

    IOChannelRequester* _iochanRequester;

    ConnectionThread* _connectionThread;

    // friend class ServerSocketConnectionThread;

    int _keepAliveIdleSecs;

    bool _nonBlocking;

    /**
     * No assignment.
     */
    ServerSocket& operator=(const ServerSocket&);

};

}}	// namespace nidas namespace core

#endif
