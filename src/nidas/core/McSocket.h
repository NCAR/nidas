/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_MCSOCKET_H
#define NIDAS_CORE_MCSOCKET_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/DOMable.h>
#include <nidas/util/McSocket.h>

#include <string>
#include <iostream>

namespace nidas { namespace core {

/**
 * Implementation of an IOChannel, using McSocket to establish connections
 */
class McSocket: public IOChannel, public nidas::util::McSocket {

public:

    /**
     * Constructor.
     */
    McSocket();

    /**
     * Copy constructor. Should only be called before nidas::util::Socket
     * is connected.
     */
    McSocket(const McSocket&);

    /**
     * Copy constructor, with a new connnected nidas::util::Socket
     */
    McSocket(const McSocket&,nidas::util::Socket*);

    ~McSocket() { delete socket; }

    McSocket* clone() const;

    void setRequestNumber(int val) {
    	nidas::util::McSocket::setRequestNumber(val);
    }

    int getRequestNumber() const {
    	return nidas::util::McSocket::getRequestNumber();
    }

    /**
     * Does this McSocket request connections, or does it
     * listen for incoming connections.
     */
    bool isRequester() const { return amRequester; }

    void setRequester(bool val) { amRequester = val; }

    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    void requestConnection(ConnectionRequester* service)
    	throw(nidas::util::IOException);

    IOChannel* connect() throw(nidas::util::IOException);

    virtual bool isNewFile() const { return newFile; }

    void connected(nidas::util::Socket* sock);

    nidas::util::Inet4Address getRemoteInet4Address() const throw();

    void setKeepAliveIdleSecs(int val) throw (nidas::util::IOException)
    {
	if (socket) socket->setKeepAliveIdleSecs(val);
        keepAliveIdleSecs = val;
    }

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
	return socket->send(buf,len,MSG_NOSIGNAL);

    }

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

    void close() throw (nidas::util::IOException);

    int getFd() const;

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement*
        toDOMParent(xercesc::DOMElement* parent)
                throw(xercesc::DOMException);

    xercesc::DOMElement*
        toDOMElement(xercesc::DOMElement* node)
                throw(xercesc::DOMException);

private:
    nidas::util::Socket* socket;

    std::string name;

    ConnectionRequester* connectionRequester;

    bool amRequester;

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

}}	// namespace nidas namespace core

#endif
