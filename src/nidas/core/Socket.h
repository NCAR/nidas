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
     * Copy constructor.
     */
    Socket(const Socket& x);

    /**
     * Constructor from a connected nidas::util::Socket.
     * @param sock Pointer to the connected nidas::util::Socket.
     * Socket will own the pointer and will delete it
     * it its destructor.
     */
    Socket(nidas::util::Socket* sock);

    ~Socket();

    Socket* clone() const;

    void requestConnection(ConnectionRequester* service)
    	throw(nidas::util::IOException);

    IOChannel* connect() throw(nidas::util::IOException);

    virtual bool isNewFile() const { return newFile; }

    /**
     * Do setKeepAliveIdleSecs(int secs) on underlying socket.
     */
    void setKeepAliveIdleSecs(int val) throw (nidas::util::IOException)
    {
	keepAliveIdleSecs = val;
	if (socket) socket->setKeepAliveIdleSecs(val);
    }

    /**
     * Return getKeepAliveIdleSecs() on underlying socket.
     */
    int getKeepAliveIdleSecs() const throw (nidas::util::IOException)
    {
	if (socket) return socket->getKeepAliveIdleSecs();
	return keepAliveIdleSecs;
    }

    /**
     * Do setNonBlocking(val) on underlying socket.
     */
    void setNonBlocking(bool val) throw (nidas::util::IOException)
    {
	nonBlocking = val;
	if (socket) socket->setNonBlocking(val);
    }

    /**
     * Return isNonBlocking() of underlying socket.
     */
    bool isNonBlocking() const throw (nidas::util::IOException)
    {
	if (socket) return socket->isNonBlocking();
	return nonBlocking;
    }

    size_t getBufferSize() const throw();

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (nidas::util::IOException);

    /**
     * Do the actual hardware write.
     */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    {
	// std::cerr << "nidas::core::Socket::write, len=" << len << std::endl;
        dsm_time_t tnow = getSystemTime();
        if (lastWrite > tnow) lastWrite = tnow; // system clock adjustment
        if (tnow - lastWrite < minWriteInterval) return 0;
        lastWrite = tnow;
	return socket->send(buf,len, MSG_NOSIGNAL);

    }

    void close() throw (nidas::util::IOException)
    {
        if (socket) socket->close();
    }

    int getFd() const
    {
        if (socket) return socket->getFd();
	return -1;
    }

    const std::string& getName() const { return name; }

    void setName(const std::string& val) { name = val; }

    nidas::util::Inet4Address getRemoteInet4Address() const throw();

    const nidas::util::SocketAddress& getRemoteSocketAddress() const throw();

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
    void setMinWriteInterval(int val) {
        minWriteInterval = val;
    }

    int getMinWriteInterval() const {
        return minWriteInterval;
    }


private:

    friend class ClientSocketConnectionThread;

    void connectionThreadFinished();

    std::auto_ptr<nidas::util::SocketAddress> remoteSockAddr;

    nidas::util::Socket* socket;

    std::string name;

    ConnectionRequester* connectionRequester;

    nidas::util::Thread* connectionThread;

    nidas::util::Mutex connectionMutex;

    bool firstRead;

    bool newFile;

    int keepAliveIdleSecs;

    /**
     * Minimum write interval in microseconds so we don't flood network.
     */
    int minWriteInterval;

    /**
     * Time of last physical write.
     */
    dsm_time_t lastWrite;

    bool nonBlocking;


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

    /**
     * Copy constructor.
     */
    ServerSocket(const ServerSocket& x);

    ~ServerSocket();

    ServerSocket* clone() const;

    void requestConnection(ConnectionRequester* service)
    	throw(nidas::util::IOException);

    IOChannel* connect() throw(nidas::util::IOException);

    const std::string& getName() const { return name; }

    void setName(const std::string& val) { name = val; }

    int getFd() const
    {
        if (servSock) return servSock->getFd();
	return -1;
    }


    /**
     * Set the value of keepAliveIdleSecs.  This is set on each
     * accepted socket connection. It does not pertain to the socket
     * which is waiting for connections.
     */
    void setKeepAliveIdleSecs(int val) throw (nidas::util::IOException)
    {
	keepAliveIdleSecs = val;
    }

    /**
     * Return keepAliveIdleSecs for this ServerSocket.
     */
    int getKeepAliveIdleSecs() const throw (nidas::util::IOException)
    {
	return keepAliveIdleSecs;
    }

    /**
     * 
     */
    void setNonBlocking(bool val) throw (nidas::util::IOException)
    {
	nonBlocking = val;
    }

    /**
     * 
     */
    bool isNonBlocking() const throw (nidas::util::IOException)
    {
	return nonBlocking;
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
    * ServerSocket will never be called to do an actual write.
    */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
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
        minWriteInterval = val;
    }

    int getMinWriteInterval() const {
        return minWriteInterval;
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

private:

    std::auto_ptr<nidas::util::SocketAddress> localSockAddr;

    std::string name;

    nidas::util::ServerSocket* servSock;

    ConnectionRequester* connectionRequester;

    nidas::util::Thread* connectionThread;

    friend class ServerSocketConnectionThread;

    int keepAliveIdleSecs;

    /**
     * Minimum write interval in microseconds so we don't flood network.
     */
    int minWriteInterval;

    bool nonBlocking;

};

class ServerSocketConnectionThread: public nidas::util::Thread
{
public:
    ServerSocketConnectionThread(ServerSocket& sock):
    	Thread("ServerSocketConnectionThread"),socket(sock) {}

    int run() throw(nidas::util::IOException);

protected:
    ServerSocket& socket;
};

class ClientSocketConnectionThread: public nidas::util::Thread
{
public:
    ClientSocketConnectionThread(Socket& sock):
    	Thread("ClientSocketConnectionThread"),socket(sock) {}

    int run() throw(nidas::util::IOException);

protected:
    Socket socket;  // copy of original
};

}}	// namespace nidas namespace core

#endif
