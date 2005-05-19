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
     * @param sock Pointer to the connected atdUtil::Socket. dsm::Socket
     *    will make a copy of the connected atdUtil::Socket and will
     *    not own the sock pointer.
     */
    Socket(const atdUtil::Socket* sock);

    ~Socket();

    void requestConnection(ConnectionRequester* service,int pseudoPort)
    	throw(atdUtil::IOException);

    IOChannel* clone() const;

    /**
    * Do the actual hardware read.
    */
    size_t read(void* buf, size_t len) throw (atdUtil::IOException)
    {
	return socket->recv(buf,len);
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (atdUtil::IOException)
    {
	return socket->send(buf,len);
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
    atdUtil::Inet4SocketAddress saddr;

    atdUtil::Socket* socket;

    std::string name;
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
     * Copy constructor.
     */
    ServerSocket(const ServerSocket& x);

    ~ServerSocket();

    void requestConnection(ConnectionRequester* service,int pseudoPort)
    	throw(atdUtil::IOException);

    IOChannel* clone() const;

    const std::string& getName() const { return name; }

    void setName(const std::string& val) { name = val; }

    int getFd() const
    {
        if (socket) return socket->getFd();
	return -1;
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

    atdUtil::ServerSocket* socket;

    ConnectionRequester* connectionRequester;

    atdUtil::Thread* thread;

    friend class ServerSocketConnectionThread;

};

class ServerSocketConnectionThread: public atdUtil::Thread
{
public:
    ServerSocketConnectionThread(ServerSocket& sock):
    	Thread("ServerSocketConnectionThread"),ssock(sock) {}

    int run() throw(atdUtil::IOException);

protected:
    ServerSocket& ssock;
};

}

#endif
