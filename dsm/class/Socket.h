/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SOCKET_H
#define DSM_SOCKET_H

#include <IOChannel.h>
#include <DOMable.h>
#include <atdUtil/Socket.h>
#include <atdUtil/Thread.h>

#include <string>
#include <iostream>

namespace dsm {

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
     * Constructor from a connected atdUtil::Socket.
     * @param sock Pointer to the connected atdUtil::Socket.
     * Socket will own the pointer and will delete it
     * it its destructor.
     */
    Socket(atdUtil::Socket* sock);

    ~Socket();

    Socket* clone() const;

    void requestConnection(ConnectionRequester* service,int pseudoPort)
    	throw(atdUtil::IOException);

    IOChannel* connect(int pseudoPort) throw(atdUtil::IOException);

    virtual bool isNewFile() const { return newFile; }

    /**
     * Do setKeepAliveIdleSecs(int secs) on underlying socket.
     */
    void setKeepAliveIdleSecs(int val) throw (atdUtil::IOException)
    {
	keepAliveIdleSecs = val;
	if (socket) socket->setKeepAliveIdleSecs(val);
    }

    /**
     * Return getKeepAliveIdleSecs() on underlying socket.
     */
    int getKeepAliveIdleSecs() const throw (atdUtil::IOException)
    {
	if (socket) return socket->getKeepAliveIdleSecs();
	return keepAliveIdleSecs;
    }

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (atdUtil::IOException);

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (atdUtil::IOException)
    {
	// std::cerr << "dsm::Socket::write, len=" << len << std::endl;
	return socket->send(buf,len,MSG_DONTWAIT | MSG_NOSIGNAL);
    }

    void close() throw (atdUtil::IOException)
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

    atdUtil::Inet4Address getRemoteInet4Address() const throw();

    /**
     * Create either a Socket or a McSocket from a DOMElement.
     */
    static IOChannel* createSocket(const xercesc::DOMElement*)
        throw(atdUtil::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
        throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
        toDOMParent(xercesc::DOMElement* parent)
                throw(xercesc::DOMException);

    xercesc::DOMElement*
        toDOMElement(xercesc::DOMElement* node)
                throw(xercesc::DOMException);

protected:
    std::auto_ptr<atdUtil::SocketAddress> remoteSockAddr;

    atdUtil::Socket* socket;

    std::string name;

    bool firstRead;

    bool newFile;

    int keepAliveIdleSecs;
};

/**
 * Implementation of an IOChannel, over a ServerSocket.
 */
class ServerSocket: public IOChannel {

public:

    /**
     * Constructor.
     */
    ServerSocket(int port = 0);

    /**
     * Copy constructor.
     */
    ServerSocket(const ServerSocket& x);

    ~ServerSocket();

    ServerSocket* clone() const;

    void requestConnection(ConnectionRequester* service,int pseudoPort)
    	throw(atdUtil::IOException);

    IOChannel* connect(int pseudoPort) throw(atdUtil::IOException);

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
    void setKeepAliveIdleSecs(int val) throw (atdUtil::IOException)
    {
	keepAliveIdleSecs = val;
    }

    /**
     * Return keepAliveIdleSecs for this ServerSocket.
     */
    int getKeepAliveIdleSecs() const throw (atdUtil::IOException)
    {
	return keepAliveIdleSecs;
    }

    /**
    * ServerSocket will never be called to do an actual read.
    */
    size_t read(void* buf, size_t len) throw (atdUtil::IOException)
    {
	assert(false);
	return 0;
    }

    /**
    * ServerSocket will never be called to do an actual write.
    */
    size_t write(const void* buf, size_t len) throw (atdUtil::IOException)
    {
	assert(false);
	return 0;
    }

    void close() throw (atdUtil::IOException)
    {
        if (servSock) servSock->close();
    }

    /**
     * Create either a Socket or a McSocket from a DOMElement.
     */
    static IOChannel* createSocket(const xercesc::DOMElement*)
        throw(atdUtil::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
        throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
        toDOMParent(xercesc::DOMElement* parent)
                throw(xercesc::DOMException);

    xercesc::DOMElement*
        toDOMElement(xercesc::DOMElement* node)
                throw(xercesc::DOMException);

protected:
    int port;

    std::string name;

    atdUtil::ServerSocket* servSock;

    ConnectionRequester* connectionRequester;

    atdUtil::Thread* thread;

    friend class ServerSocketConnectionThread;

    int keepAliveIdleSecs;

};

class ServerSocketConnectionThread: public atdUtil::Thread
{
public:
    ServerSocketConnectionThread(ServerSocket& sock):
    	Thread("ServerSocketConnectionThread"),socket(sock) {}

    int run() throw(atdUtil::IOException);

protected:
    ServerSocket& socket;
};

}

#endif
