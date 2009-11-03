/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-03-26 22:35:58 -0600 (Thu, 26 Mar 2009) $

    $LastChangedRevision: 4548 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/Socket.h $
 ********************************************************************

*/

#ifndef NIDAS_CORE_DATAGRAMSOCKET_H
#define NIDAS_CORE_DATAGRAMSOCKET_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/DOMable.h>
#include <nidas/util/Socket.h>

#include <string>
#include <iostream>
#include <memory>  // auto_ptr<>

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

    DatagramSocket(nidas::util::DatagramSocket* sock);

    ~DatagramSocket();

    DatagramSocket* clone() const;

    void requestConnection(IOChannelRequester* service)
    	throw(nidas::util::IOException);

    IOChannel* connect() throw(nidas::util::IOException);

    bool isNewInput() const { return _newInput; }

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
#ifdef CHECK_MIN_WRITE_INTERVAL
        dsm_time_t tnow = getSystemTime();
        if (_lastWrite > tnow) _lastWrite = tnow; // system clock adjustment
        if (tnow - _lastWrite < _minWriteInterval) return 0;
        _lastWrite = tnow;
#endif
        return _nusocket->send(buf,len,0);
    }

    /**
     * Do the actual hardware write.
     */
    size_t write(const struct iovec* iov, int iovcnt) throw (nidas::util::IOException)
    {
#ifdef CHECK_MIN_WRITE_INTERVAL
        dsm_time_t tnow = getSystemTime();
        if (_lastWrite > tnow) _lastWrite = tnow; // system clock adjustment
        if (tnow - _lastWrite < _minWriteInterval) return 0;
        _lastWrite = tnow;
#endif
        return _nusocket->send(iov,iovcnt,0);
    }

    void close() throw (nidas::util::IOException)
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

    nidas::util::Inet4Address getRemoteInet4Address();

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

    const nidas::util::SocketAddress& getRemoteSocketAddress()
        throw(nidas::util::UnknownHostException);

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

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

private:

    std::auto_ptr<nidas::util::SocketAddress> _remoteSockAddr;

    std::string _remoteHost;

    unsigned short _remotePort;

    std::string _unixPath;

    nidas::util::DatagramSocket* _nusocket;

    std::string _name;

    IOChannelRequester* _iochanRequester;

    bool _firstRead;

    bool _newInput;

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
