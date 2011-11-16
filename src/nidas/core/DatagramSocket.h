// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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

    DatagramSocket& operator=(const DatagramSocket& rhs);

    DatagramSocket(nidas::util::DatagramSocket* sock);

    ~DatagramSocket();

    DatagramSocket* clone() const;

    void requestConnection(IOChannelRequester* service)
    	throw(nidas::util::IOException);

    IOChannel* connect() throw(nidas::util::IOException);

    bool isNewInput() const { return false; }

    bool writeNidasHeader() const { return false; }

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
        return _nusocket->recv(buf,len);
    }

    /**
     * Do the actual hardware write.
     */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    {
        return _nusocket->send(buf,len,0);
    }

    /**
     * Do the actual hardware write.
     */
    size_t write(const struct iovec* iov, int iovcnt) throw (nidas::util::IOException)
    {
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

    const nidas::util::SocketAddress& getSocketAddress()
        throw(nidas::util::UnknownHostException);

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

    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

private:

    std::auto_ptr<nidas::util::SocketAddress> _sockAddr;

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
