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
class McSocket: public IOChannel, public nidas::util::McSocket<nidas::util::Socket> {

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

    ~McSocket() { delete _socket; }

    McSocket* clone() const;

    void setRequestNumber(int val) {
    	nidas::util::McSocket<nidas::util::Socket>::setRequestNumber(val);
    }

    int getRequestNumber() const {
    	return nidas::util::McSocket<nidas::util::Socket>::getRequestNumber();
    }

    /**
     * Does this McSocket request connections, or does it
     * listen for incoming connections.
     */
    bool isRequester() const { return _amRequester; }

    void setRequester(bool val) { _amRequester = val; }

    void setName(const std::string& val) { _name = val; }

    const std::string& getName() const { return _name; }

    void requestConnection(ConnectionRequester* service)
    	throw(nidas::util::IOException);

    IOChannel* connect() throw(nidas::util::IOException);

    virtual bool isNewInput() const { return _newInput; }

    void connected(nidas::util::Socket* sock);

    nidas::util::Inet4Address getRemoteInet4Address();

    void setKeepAliveIdleSecs(int val) throw (nidas::util::IOException)
    {
	if (_socket) _socket->setKeepAliveIdleSecs(val);
        _keepAliveIdleSecs = val;
    }

    int getKeepAliveIdleSecs() const throw (nidas::util::IOException)
    {
	if (_socket) return _socket->getKeepAliveIdleSecs();
        return _keepAliveIdleSecs;
    }

    std::list<nidas::util::Inet4NetworkInterface> getInterfaces() const
        throw(nidas::util::IOException)
    {
        if (_socket) return _socket->getInterfaces();
        return std::list<nidas::util::Inet4NetworkInterface>();
    }

    /**
     * Do setNonBlocking(val) on underlying socket.
     */
    void setNonBlocking(bool val) throw (nidas::util::IOException)
    {
	_nonBlocking = val;
	if (_socket) _socket->setNonBlocking(val);
    }

    /**
     * Return isNonBlocking() of underlying socket.
     */
    bool isNonBlocking() const throw (nidas::util::IOException)
    {
	if (_socket) return _socket->isNonBlocking();
	return _nonBlocking;
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
        if (_lastWrite > tnow) _lastWrite = tnow; // system clock adjustment
        if (tnow - _lastWrite < _minWriteInterval) return 0;
        _lastWrite = tnow;
	return _socket->send(buf,len,MSG_NOSIGNAL);

    }

    /**
     * Set the minimum write interval in microseconds so we don't
     * flood the network.
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

    int getFd() const;

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

private:
    nidas::util::Socket* _socket;

    std::string _name;

    ConnectionRequester* _connectionRequester;

    bool _amRequester;

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

};

}}	// namespace nidas namespace core

#endif
