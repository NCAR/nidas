// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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


#include "BluetoothRFCommSocket.h"

#ifdef HAVE_BLUETOOTH_RFCOMM_H

#include "Logger.h"
#include "IOTimeoutException.h"
#include "EOFException.h"
#include "time_constants.h"

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <cassert>

#ifdef DEBUG
#include <iostream>
#endif

#ifdef HAVE_PPOLL
#include <poll.h>
#else
#include <sys/select.h>
#endif

using namespace nidas::util;
using namespace std;

BluetoothRFCommSocket::BluetoothRFCommSocket():
    _fd(-1),_localaddr(0),_remoteaddr(0),
    _hasTimeout(false),_timeout()
{
    if ((_fd = ::socket(AF_BLUETOOTH,SOCK_STREAM, BTPROTO_RFCOMM)) < 0)
	throw IOException("BluetoothRFCommSocket","open",errno);
    getLocalAddr();
    _remoteaddr = _localaddr->clone();	// not connected yet
}

BluetoothRFCommSocket::BluetoothRFCommSocket(int fda, const SocketAddress& raddr):
    _fd(fda),_localaddr(0),_remoteaddr(raddr.clone()),
    _hasTimeout(false),_timeout()
{
    getLocalAddr();
}

/* copy constructor */
BluetoothRFCommSocket::BluetoothRFCommSocket(const BluetoothRFCommSocket& x):
    _fd(x._fd), _localaddr(x._localaddr->clone()),
    _remoteaddr(x._remoteaddr->clone()),
    _hasTimeout(x._hasTimeout),_timeout(x._timeout)
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
    _timeout.tv_sec = val / MSECS_PER_SEC;
    _timeout.tv_nsec = (val % MSECS_PER_SEC) * NSECS_PER_MSEC;
    _hasTimeout = val > 0;
}

int BluetoothRFCommSocket::getTimeout() const
{
    return _timeout.tv_sec * MSECS_PER_SEC + _timeout.tv_nsec / NSECS_PER_MSEC;
}

void BluetoothRFCommSocket::close()
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
{
    BluetoothAddress addr = BluetoothAddress::getByName(host);
    connect(addr,port);
}

void BluetoothRFCommSocket::connect(const BluetoothAddress& addr, int port)
{
    BluetoothRFCommSocketAddress sockaddr(addr,port);
    connect(sockaddr);
}

void BluetoothRFCommSocket::connect(const SocketAddress& addr)
{
    if (_fd < 0 && (_fd = ::socket(AF_BLUETOOTH,SOCK_STREAM, BTPROTO_RFCOMM)) < 0)
	throw IOException("BluetoothRFCommSocket","open",errno);

    if (::connect(_fd,addr.getConstSockAddrPtr(),addr.getSockAddrLen()) < 0) {
	throw IOException(addr.toAddressString(),"connect",errno);
    }
    getLocalAddr();
    getRemoteAddr();
}


void BluetoothRFCommSocket::bind(int port)
{
    BluetoothAddress addr;	// default
    bind(addr,port);
}

void BluetoothRFCommSocket::bind(const BluetoothAddress& addr, int port)
{
    BluetoothRFCommSocketAddress sockaddr(addr,port);
    bind(sockaddr);
}

void BluetoothRFCommSocket::bind(const SocketAddress& sockaddr)
{
    if (_fd < 0 && (_fd = ::socket(AF_BLUETOOTH,SOCK_STREAM,BTPROTO_RFCOMM)) < 0)
	throw IOException("BluetoothRFCommSocket","open",errno);

    if (::bind(_fd,sockaddr.getConstSockAddrPtr(),
        sockaddr.getSockAddrLen()) < 0) {
	throw IOException(sockaddr.toAddressString(),"bind",errno);
    }
    // get actual local address
    getLocalAddr();
}

void BluetoothRFCommSocket::listen()
{
    if (_fd < 0)
	throw IOException("BluetoothRFCommSocket","listen",EBADF);

    if (::listen(_fd,1) < 0)
	throw IOException(_localaddr->toAddressString(),"listen",errno);
}

BluetoothRFCommSocket* BluetoothRFCommSocket::accept()
{
    if (_fd < 0)
	throw IOException("BluetoothRFCommSocket","accept",EBADF);

    setNonBlocking(true);

#ifdef HAVE_PPOLL
    struct pollfd fds;
    fds.fd = _fd;
#ifdef POLLRDHUP
    fds.events = POLLIN | POLLRDHUP;
#else
    fds.events = POLLIN;
#endif
#else
    fd_set rfdset,efdset;
    FD_ZERO(&rfdset);
    FD_ZERO(&efdset);
#endif

    sigset_t sigmask;
    // get current signal mask
    pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
    // unblock SIGUSR1 in pselect/ppoll
    sigdelset(&sigmask,SIGUSR1);

    for ( ; ; ) {

        int res;
#ifdef HAVE_PPOLL
        res = ::ppoll(&fds,1,NULL,&sigmask);
        if (res < 0)
            throw IOException(_localaddr->toAddressString(),"ppoll",errno);

        if (fds.revents & POLLERR)
            throw IOException(_localaddr->toAddressString(),"accept",errno);

#ifdef POLLRDHUP
        if (fds.revents & (POLLHUP | POLLRDHUP))
#else
        if (fds.revents & POLLHUP)
#endif
            WLOG(("%s POLLHUP",_localaddr->toAddressString().c_str()));

#else
        if ((res = ::pselect(_fd+1,&rfdset,0,&efdset,NULL,&sigmask)) < 0)
            throw IOException(_localaddr->toAddressString(),"pselect",errno);

        if (FD_ISSET(_fd,&efdset))
            throw IOException(_localaddr->toAddressString(),"accept",errno);
#endif

        struct sockaddr_rc tmpaddr;
        socklen_t slen = sizeof(tmpaddr);
        memset(&tmpaddr,0,slen);
        int newfd;
        if ((newfd = ::accept(_fd,(struct sockaddr*)&tmpaddr,&slen)) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED)
                    continue;
            throw IOException(_localaddr->toAddressString(),"accept",errno);
        }
        return new BluetoothRFCommSocket(newfd,BluetoothRFCommSocketAddress(&tmpaddr));
    }
    return 0;
}

void BluetoothRFCommSocket::getLocalAddr()
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

void BluetoothRFCommSocket::getRemoteAddr()
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

void BluetoothRFCommSocket::setNonBlocking(bool val)
{
    int flags;
    /* set io to non-blocking, so network jams don't hang us up */
    if ((flags = ::fcntl(_fd, F_GETFL, 0)) < 0) {
        throw IOException(_localaddr->toAddressString(),
		"fcntl(...,F_GETFL,...)",errno);
    }
    if (val) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;

    if (::fcntl(_fd, F_SETFL, flags) < 0) {
        throw IOException(_localaddr->toAddressString(),
		"fcntl(...,F_SETFL,O_NONBLOCK)",errno);
    }
    ILOG(("%s: setNonBlocking(%s)",_localaddr->toAddressString().c_str(),
	(val ? "true" : "false")));
}

bool BluetoothRFCommSocket::isNonBlocking() const
{
    int flags;
    /* set io to non-blocking, so network jams don't hang us up */
    if ((flags = ::fcntl(_fd, F_GETFL, 0)) < 0) {
        throw IOException(_localaddr->toAddressString(),
		"fcntl(...,F_GETFL,...)",errno);
    }
    return (flags & O_NONBLOCK) != 0;
}

size_t BluetoothRFCommSocket::recv(void* buf, size_t len, int flags)
{
    ssize_t res;
    if (_hasTimeout) {

        // get the existing signal mask
        sigset_t sigmask;
        pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
        // unblock SIGUSR1 in pselect/ppoll
        sigdelset(&sigmask,SIGUSR1);

#ifdef HAVE_PPOLL
        struct pollfd fds;
        fds.fd =  _fd;
#ifdef POLLRDHUP
        fds.events = POLLIN | POLLRDHUP;
#else
        fds.events = POLLIN;
#endif
#else
        fd_set fdset;
	FD_ZERO(&fdset);
        assert(_fd >= 0 && _fd < FD_SETSIZE);     // FD_SETSIZE=1024
	FD_SET(_fd, &fdset);
#endif

        int res;
#ifdef HAVE_PPOLL
        res = ::ppoll(&fds,1,&_timeout,&sigmask);
        if (res < 0)
	    throw IOException(_localaddr->toAddressString(),"receive",errno);

	if (res == 0)
	    throw IOTimeoutException(_localaddr->toAddressString(),"receive");

        if (fds.revents & POLLERR)
	    throw IOException(_localaddr->toAddressString(),"receive",errno);

#ifdef POLLRDHUP
        if (fds.revents & (POLLHUP | POLLRDHUP))
#else
        if (fds.revents & POLLHUP)
#endif
            WLOG(("%s POLLHUP",_localaddr->toAddressString().c_str()));

#else
	if ((res = ::pselect(_fd+1,&fdset,0,0,&_timeout,&sigmask)) < 0)
	    throw IOException(_localaddr->toAddressString(),"receive",errno);

	if (res == 0)
	    throw IOTimeoutException(_localaddr->toAddressString(),"receive");
#endif
    }
    if ((res = ::recv(_fd,buf,len,flags)) <= 0) {
	if (res < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
	    throw IOException(_localaddr->toAddressString(),"recv",errno);
	}
	else throw EOFException(_localaddr->toAddressString(), "recv");
    }
    return res;
}

size_t BluetoothRFCommSocket::send(const void* buf, size_t len, int flags)
{
    ssize_t res;
    if ((res = ::send(_fd,buf,len,flags)) < 0) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
	throw IOException(_remoteaddr->toAddressString(),"send",errno);
    }
    return res;
}

size_t BluetoothRFCommSocket::send(const struct iovec* iov, int iovcnt, int flags)
{
    ssize_t res;
    struct msghdr msghdr;
    memset(&msghdr,0,sizeof(msghdr));
    msghdr.msg_iov = const_cast<struct iovec*>(iov);
    msghdr.msg_iovlen = iovcnt;
    if ((res = ::sendmsg(_fd,&msghdr,flags)) < 0) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
	throw IOException(_remoteaddr->toAddressString(),"send",errno);
    }
    return res;
}

void BluetoothRFCommSocket::sendall(const void* buf, size_t len, int flags)
{
    const char* cbuf = (const char*)buf;
    const char* eob = cbuf + len;

    while (cbuf < eob) {
	ssize_t res;
	if ((res = ::send(_fd,cbuf,len,flags)) < 0) {
	    // if (errno == EAGAIN || errno == EWOULDBLOCK) sleep?
	    throw IOException(_remoteaddr->toAddressString(),"send",errno);
	}
	cbuf += res;
	len -= res;
    }
}

#endif
