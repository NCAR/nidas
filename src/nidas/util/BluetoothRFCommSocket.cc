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


#include <nidas/util/BluetoothRFCommSocket.h>

#ifdef HAVE_BLUETOOTH_RFCOMM_H

#include <nidas/util/Logger.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/EOFException.h>
#include <unistd.h>
#include <fcntl.h>
                                                                                
#ifdef DEBUG
#include <iostream>
#endif

using namespace nidas::util;
using namespace std;

BluetoothRFCommSocket::BluetoothRFCommSocket() throw(IOException):
    _fd(-1),_localaddr(0),_remoteaddr(0),
    _hasTimeout(false),_timeout(),_fdset()
{
    if ((_fd = ::socket(AF_BLUETOOTH,SOCK_STREAM, BTPROTO_RFCOMM)) < 0)
	throw IOException("BluetoothRFCommSocket","open",errno);
    getLocalAddr();
    _remoteaddr = _localaddr->clone();	// not connected yet
}

BluetoothRFCommSocket::BluetoothRFCommSocket(int fda, const SocketAddress& raddr)
	throw(IOException) :
    _fd(fda),_localaddr(0),_remoteaddr(raddr.clone()),
    _hasTimeout(false),_timeout(),_fdset()
{
    getLocalAddr();
}

/* copy constructor */
BluetoothRFCommSocket::BluetoothRFCommSocket(const BluetoothRFCommSocket& x):
    _fd(x._fd), _localaddr(x._localaddr->clone()),
    _remoteaddr(x._remoteaddr->clone()),
    _hasTimeout(x._hasTimeout),_timeout(x._timeout),_fdset()
{
}

/* assignment operator */
BluetoothRFCommSocket& BluetoothRFCommSocket::operator =(const BluetoothRFCommSocket& rhs)
{
    if (this != &rhs) {
        _fd = rhs._fd;
        delete _localaddr;
        _localaddr = rhs._localaddr->clone();
        delete _remoteaddr;
        _remoteaddr = rhs._remoteaddr->clone();
        _hasTimeout = rhs._hasTimeout;
        _timeout = rhs._timeout;
        FD_ZERO(&_fdset);
    }
    return *this;
}

BluetoothRFCommSocket::~BluetoothRFCommSocket() throw()
{
    delete _localaddr;
    delete _remoteaddr;
}

void BluetoothRFCommSocket::setTimeout(int val)
{
    _timeout.tv_sec = val / 1000;
    _timeout.tv_usec = (val % 1000) * 1000;
    _hasTimeout = val > 0;
}

int BluetoothRFCommSocket::getTimeout() const
{
    return _timeout.tv_sec * 1000 + _timeout.tv_usec / 1000;
}

void BluetoothRFCommSocket::close() throw(IOException) 
{

#ifdef DEBUG
    cerr << "closing, local=" << getLocalSocketAddress().toString()
	 << " remote=" << getRemoteSocketAddress().toString() << endl;
#endif
    int fd = _fd;
    _fd = -1;
    if (fd >= 0 && ::close(fd) < 0) 
    	throw IOException("BluetoothRFCommSocket","close",errno);
}

void BluetoothRFCommSocket::connect(const std::string& host, int port)
	throw(UnknownHostException,IOException)
{
    BluetoothAddress addr = BluetoothAddress::getByName(host);
    connect(addr,port);
}

void BluetoothRFCommSocket::connect(const BluetoothAddress& addr,int port)
	throw(IOException)
{
    BluetoothRFCommSocketAddress sockaddr(addr,port);
    connect(sockaddr);
}

void BluetoothRFCommSocket::connect(const SocketAddress& addr)
	throw(IOException)
{
    if (_fd < 0 && (_fd = ::socket(AF_BLUETOOTH,SOCK_STREAM, BTPROTO_RFCOMM)) < 0)
	throw IOException("BluetoothRFCommSocket","open",errno);

    if (::connect(_fd,addr.getConstSockAddrPtr(),addr.getSockAddrLen()) < 0) {
	int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
	throw IOException(addr.toAddressString(),"connect",ierr);
    }
    getLocalAddr();	
    getRemoteAddr();
}


void BluetoothRFCommSocket::bind(int port) throw(IOException)
{
    BluetoothAddress addr;	// default
    bind(addr,port);
}

void BluetoothRFCommSocket::bind(const BluetoothAddress& addr,int port)
	throw(IOException)
{
    BluetoothRFCommSocketAddress sockaddr(addr,port);
    bind(sockaddr);
}

void BluetoothRFCommSocket::bind(const SocketAddress& sockaddr)
	throw(IOException)
{
    if (_fd < 0 && (_fd = ::socket(AF_BLUETOOTH,SOCK_STREAM,BTPROTO_RFCOMM)) < 0)
	throw IOException("BluetoothRFCommSocket","open",errno);

    if (::bind(_fd,sockaddr.getConstSockAddrPtr(),
    	sockaddr.getSockAddrLen()) < 0) {
	int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
	throw IOException(sockaddr.toAddressString(),"bind",ierr);
    }
    // get actual local address
    getLocalAddr();
}

void BluetoothRFCommSocket::listen() throw(IOException)
{
    if (_fd < 0 && (_fd = ::socket(AF_BLUETOOTH,SOCK_STREAM,BTPROTO_RFCOMM)) < 0)
	throw IOException("BluetoothRFCommSocket","open",errno);
    if (::listen(_fd,1) < 0) {
	int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
	throw IOException(_localaddr->toAddressString(),"listen",ierr);
    }
}

BluetoothRFCommSocket* BluetoothRFCommSocket::accept() throw(IOException)
{
    if (_fd < 0 && (_fd = ::socket(AF_BLUETOOTH,SOCK_STREAM,BTPROTO_RFCOMM)) < 0)
	throw IOException("BluetoothRFCommSocket","open",errno);

    struct sockaddr_rc tmpaddr;
    socklen_t slen = sizeof(tmpaddr);
    memset(&tmpaddr,0,slen);
    int newfd;
    if ((newfd = ::accept(_fd,(struct sockaddr*)&tmpaddr,&slen)) < 0)
            throw IOException("BluetoothRFCommSocket","accept",errno);
    return new BluetoothRFCommSocket(newfd,BluetoothRFCommSocketAddress(&tmpaddr));
    return 0;
}

void BluetoothRFCommSocket::getLocalAddr() throw(IOException)
{
    SocketAddress* newaddr;
    struct sockaddr_rc tmpaddr;
    socklen_t slen = sizeof(tmpaddr);
    if (::getsockname(_fd,(struct sockaddr*)&tmpaddr,&slen) < 0)
        throw IOException("BluetoothRFCommSocket","getsockname",errno);
    newaddr = new BluetoothRFCommSocketAddress(&tmpaddr);
    delete _localaddr;
    _localaddr = newaddr;
}

void BluetoothRFCommSocket::getRemoteAddr() throw(IOException)
{
    SocketAddress* newaddr;
    struct sockaddr_rc tmpaddr;
    socklen_t slen = sizeof(tmpaddr);
    if (::getpeername(_fd,(struct sockaddr*)&tmpaddr,&slen) < 0)
        throw IOException("BluetoothRFCommSocket","getpeername",errno);
    newaddr = new BluetoothRFCommSocketAddress(&tmpaddr);
    delete _remoteaddr;
    _remoteaddr = newaddr;
}

void BluetoothRFCommSocket::setNonBlocking(bool val) throw(IOException)
{
    int flags;
    /* set io to non-blocking, so network jams don't hang us up */
    if ((flags = ::fcntl(_fd, F_GETFL, 0)) < 0) {
	int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
		"fcntl(...,F_GETFL,...)",ierr);
    }
    if (val) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;

    if (::fcntl(_fd, F_SETFL, flags) < 0) {
	int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
		"fcntl(...,F_SETFL,O_NONBLOCK)",ierr);
    }
    ILOG(("%s: setNonBlocking(%s)",_localaddr->toAddressString().c_str(),
	(val ? "true" : "false")));
}

bool BluetoothRFCommSocket::isNonBlocking() const throw(IOException)
{
    int flags;
    /* set io to non-blocking, so network jams don't hang us up */
    if ((flags = ::fcntl(_fd, F_GETFL, 0)) < 0) {
	int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
		"fcntl(...,F_GETFL,...)",ierr);
    }
    return (flags & O_NONBLOCK) != 0;
}

size_t BluetoothRFCommSocket::recv(void* buf, size_t len, int flags)
	throw(IOException)
{
    ssize_t res;
    if (_hasTimeout) {
	FD_ZERO(&_fdset);
	FD_SET(_fd, &_fdset);
	struct timeval tmpto = _timeout;
	if ((res = ::select(_fd+1,&_fdset,0,0,&tmpto)) < 0) {
	    int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
	    throw IOException(_localaddr->toAddressString(),"receive",ierr);
	}
	if (res == 0) 
	    throw IOTimeoutException(_localaddr->toAddressString(),"receive");
    }
    if ((res = ::recv(_fd,buf,len,flags)) <= 0) {
	if (res < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
	    int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
	    throw IOException(_localaddr->toAddressString(),"recv",ierr);
	}
	else throw EOFException(_localaddr->toAddressString(), "recv");
    }
    return res;
}

size_t BluetoothRFCommSocket::send(const void* buf, size_t len, int flags)
	throw(IOException)
{
    ssize_t res;
    if ((res = ::send(_fd,buf,len,flags)) < 0) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
	int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
	throw IOException(_remoteaddr->toAddressString(),"send",ierr);
    }
    return res;
}

size_t BluetoothRFCommSocket::send(const struct iovec* iov, int iovcnt, int flags)
	throw(IOException)
{
    ssize_t res;
    struct msghdr msghdr;
    memset(&msghdr,0,sizeof(msghdr));
    msghdr.msg_iov = const_cast<struct iovec*>(iov);
    msghdr.msg_iovlen = iovcnt;
    if ((res = ::sendmsg(_fd,&msghdr,flags)) < 0) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
	int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
	throw IOException(_remoteaddr->toAddressString(),"send",ierr);
    }
    return res;
}

void BluetoothRFCommSocket::sendall(const void* buf, size_t len, int flags)
	throw(IOException)
{
    const char* cbuf = (const char*)buf;
    const char* eob = cbuf + len;

    while (cbuf < eob) {
	ssize_t res;
	if ((res = ::send(_fd,cbuf,len,flags)) < 0) {
	    // if (errno == EAGAIN || errno == EWOULDBLOCK) sleep?
	    int ierr = errno;	// BluetoothRFCommSocketAddress::toString changes errno
	    throw IOException(_remoteaddr->toAddressString(),"send",ierr);
	}
	cbuf += res;
	len -= res;
    }
}

#endif
