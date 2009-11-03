/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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
#ifdef CHECK_MIN_WRITE_INTERVAL
        dsm_time_t tnow = getSystemTime();
        if (_lastWrite > tnow) _lastWrite = tnow; // system clock adjustment
        if (tnow - _lastWrite < _minWriteInterval) return 0;
        _lastWrite = tnow;
#endif
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
#ifdef CHECK_MIN_WRITE_INTERVAL
        dsm_time_t tnow = getSystemTime();
        if (_lastWrite > tnow) _lastWrite = tnow; // system clock adjustment
        if (tnow - _lastWrite < _minWriteInterval) return 0;
        _lastWrite = tnow;
#endif
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

    /**
     * Set the minimum write interval in microseconds so we don't
     * flood the network.
     * @param val Number of microseconds between physical writes.
     *        Default: 10000 microseconds (1/100 sec).
     */
    void setMinWriteInterval(int val)
    {
        if (_nusocket) _nusocket->setTcpNoDelay(val==0);
        _minWriteInterval = val;
    }

    int getMinWriteInterval() const {
        return _minWriteInterval;
    }

    class ConnectionThread: public nidas::util::Thread
    {
    public:
        ConnectionThread(Socket* sock);
        ~ConnectionThread();

        int run() throw(nidas::util::IOException);

        void interrupt();

    protected:
        Socket* _socket;
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

    /**
     * Minimum write interval in microseconds so we don't flood network.
     */
    int _minWriteInterval;

    /**
     * Time of last physical write.
     */
    dsm_time_t _lastWrite;

    bool _nonBlocking;

    nidas::util::Mutex _connectionMutex;

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
     * 
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
    size_t read(void* buf, size_t len) throw (nidas::util::IOException)
    {
	assert(false);
	return 0;
    }

    /**
    * ServerSocket should never be called to do an actual write.
    */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    {
	assert(false);
	return 0;
    }

    /**
     * ServerSocket should never be called to do an actual write.
     */
    size_t write(const struct iovec* iov, int iovcnt) throw (nidas::util::IOException)
    {
	assert(false);
	return 0;
    }

    /**
     * Set the minimum write interval in microseconds so we don't
     * flood the network.  This gets passed onto any sockets
     * that are connected by this ServerSocket.
     * @param val Number of microseconds between physical writes.
     *        Default: 10000 microseconds (1/100 sec).
     */
    void setMinWriteInterval(int val) {
        _minWriteInterval = val;
    }

    int getMinWriteInterval() const {
        return _minWriteInterval;
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

    protected:
        ServerSocket* _socket;
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

    /**
     * Minimum write interval in microseconds so we don't flood network.
     */
    int _minWriteInterval;

    bool _nonBlocking;

};

}}	// namespace nidas namespace core

#endif
