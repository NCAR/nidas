// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include <nidas/Config.h>   // HAVE_PPOLL

#include "Socket.h"
#include "Inet4SocketAddress.h"
#include "UnixSocketAddress.h"
#include "EOFException.h"
#include "IOTimeoutException.h"
#include "Logger.h"
#include "time_constants.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <signal.h>

#ifdef HAVE_PPOLL
#include <poll.h>
#else
#include <sys/select.h>
#endif

#include <sys/socket.h>
#include <sys/ioctl.h>  // ioctl(,SIOCGIFCONF,)
#include <net/if.h>     // ioctl(,SIOCGIFCONF,)
#include <sys/un.h>     // ioctl(,SIOCGIFCONF,)
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <linux/sockios.h>
                                                                                
#include <iostream>

using namespace nidas::util;
using namespace std;

SocketImpl::SocketImpl(int domain,int type) throw(IOException):
    _sockdomain(domain),_socktype(type),
    _localaddr(0),_remoteaddr(0),
    _fd(-1),_backlog(10),_reuseaddr(true),
    _hasTimeout(false),_timeout(),_pktInfo(false)
{
    if ((_fd = ::socket(_sockdomain,_socktype, 0)) < 0)
        throw IOException("Socket","open",errno);
    getLocalAddr();
    _remoteaddr = _localaddr->clone();  // not connected yet
}

SocketImpl::SocketImpl(int fda, const SocketAddress& raddr)
    throw(IOException) :
    _sockdomain(raddr.getFamily()),// AF_* are equal to PF_* in sys/socket.h
    _socktype(SOCK_STREAM),
    _localaddr(0),
    _remoteaddr(raddr.clone()),
    _fd(fda),
    _backlog(10),_reuseaddr(true),
    _hasTimeout(false),_timeout(),_pktInfo(false)
{
    getLocalAddr();
    // getRemoteAddr();
}

/* copy constructor */
SocketImpl::SocketImpl(const SocketImpl& x):
    _sockdomain(x._sockdomain),_socktype(x._socktype),
    _localaddr(x._localaddr->clone()),_remoteaddr(x._remoteaddr->clone()),
    _fd(x._fd),
    _backlog(x._backlog),_reuseaddr(x._reuseaddr),
    _hasTimeout(x._hasTimeout),_timeout(x._timeout),_pktInfo(x._pktInfo)
{
}

/* assignment operator */
SocketImpl& SocketImpl::operator =(const SocketImpl& rhs)
{
    if (this != &rhs) {
        _sockdomain = rhs._sockdomain;
        _socktype = rhs._socktype;
        delete _localaddr;
        _localaddr = rhs._localaddr->clone();
        delete _remoteaddr;
        _remoteaddr = rhs._remoteaddr->clone();
        _fd = rhs._fd;
        _backlog = rhs._backlog;
        _reuseaddr = rhs._reuseaddr;
        setTimeout(rhs.getTimeout());
        _pktInfo = rhs._pktInfo;
    }
    return *this;
}

SocketImpl::~SocketImpl()
{
    delete _localaddr;
    delete _remoteaddr;
}

void SocketImpl::setTimeout(int val)
{
    _timeout.tv_sec = val / MSECS_PER_SEC;
    _timeout.tv_nsec = (val % MSECS_PER_SEC) * NSECS_PER_MSEC;
    _hasTimeout = val > 0;
}

int SocketImpl::getTimeout() const
{
    return _timeout.tv_sec * MSECS_PER_SEC + _timeout.tv_nsec / NSECS_PER_MSEC;
}

void SocketImpl::close() throw(IOException) 
{

    VLOG(("closing, local=") << getLocalSocketAddress().toString()
         << " remote=" << getRemoteSocketAddress().toString());
    int fd = _fd;
    _fd = -1;
    if (fd >= 0 && ::close(fd) < 0) 
        throw IOException("Socket","close",errno);
}

void SocketImpl::connect(const std::string& host, int port)
    throw(UnknownHostException,IOException)
{
    Inet4Address addr = Inet4Address::getByName(host);
    connect(addr,port);
}

void SocketImpl::connect(const Inet4Address& addr,int port)
    throw(IOException)
{
    Inet4SocketAddress sockaddr(addr,port);
    connect(sockaddr);
}

void SocketImpl::connect(const SocketAddress& addr)
    throw(IOException)
{
    if (_fd < 0 && (_fd = ::socket(_sockdomain,_socktype, 0)) < 0)
        throw IOException("Socket","open",errno);
    // works for both AF_UNIX, AF_INET
    if (::connect(_fd,addr.getConstSockAddrPtr(),addr.getSockAddrLen()) < 0) {
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(addr.toAddressString(),"connect",ierr);
    }
    // For unbound INET sockets the local address isn't set until a connect,
    // so we do it now.
    // For UNIX sockets a getsockname() after a connect doesn't give
    // any local address info, so getLocalAddr doesn't do much.
    getLocalAddr();
    getRemoteAddr();
}


void SocketImpl::bind(int port) throw(IOException)
{
    Inet4Address addr;  // default
    bind(addr,port);
}

void SocketImpl::bind(const Inet4Address& addr,int port)
    throw(IOException)
{
    Inet4SocketAddress sockaddr(addr,port);
    bind(sockaddr);
}

void SocketImpl::bind(const SocketAddress& sockaddr)
    throw(IOException)
{
    // The range of dynamic ports on IPV4 is supposed to be is 49152 to 65536, according to RFC6335.
    // http://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xml
    //
    // Linux doesn't necessarily follow RFC6335 on this. The range in effect on a system
    // is shown in:
    //      /proc/sys/net/ipv4/ip_local_port_range
    // which on RHEL5 and Fedora 15 is 32768-61000.
    //
    // On Arcom Embedded Linux (Vipers and Vulcans) ip_local_port_range is 1024    4999.
    //
    // We could read that file, or perhaps it is available via some obscure ioctl?
    // Instead we'll hard code the warning, using NIDAS_EMBEDDED as an imperfect
    // way to detect if we're on a viper/vulcan.

#ifdef NIDAS_EMBEDDED
    if (sockaddr.getPort() >= 1024 && sockaddr.getPort() <= 4999) 
        WLOG(("%s: bind to a port number in the range 1024-4999 will fail if it has been dynamically allocated by the system for another connection. See /proc/sys/net/ipv4/ip_local_port_range",
            sockaddr.toAddressString().c_str()));
#else
    if (sockaddr.getPort() >= 32768 && sockaddr.getPort() <= 61000) 
        WLOG(("%s: bind to a port number in the range 32768-61000 will fail if it has been dynamically allocated by the system for another connection. See /proc/sys/net/ipv4/ip_local_port_range on Linux",
            sockaddr.toAddressString().c_str()));
#endif
    if (_fd < 0 && (_fd = ::socket(_sockdomain,_socktype, 0)) < 0)
        throw IOException("Socket","open",errno);
    int rval = _reuseaddr ? 1 : 0;        /* flag for setsocketopt */
    if (::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR,
                     (void *)&rval, sizeof(rval)) < 0)
    {
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(sockaddr.toAddressString(),"setsockopt SO_REUSEADDR",ierr);
    }

    if (::bind(_fd,sockaddr.getConstSockAddrPtr(),
               sockaddr.getSockAddrLen()) < 0)
    {
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(sockaddr.toAddressString(),"bind",ierr);
    }

    if (_socktype == SOCK_STREAM) listen();

    // get actual local address
    getLocalAddr();
}

void SocketImpl::listen() throw(IOException)
{
    if (_fd < 0 && (_fd = ::socket(_sockdomain,_socktype, 0)) < 0)
        throw IOException("Socket","open",errno);
    if (::listen(_fd,_backlog) < 0) {
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),"listen",ierr);
    }
}

Socket* SocketImpl::accept() throw(IOException)
{
    if (_fd < 0 && (_fd = ::socket(_sockdomain,_socktype, 0)) < 0)
        throw IOException("Socket","open",errno);

    /* This accept follows this useful bit of wisdom from man 2 accept:
     * There may not always be a connection waiting after a SIGIO is
     * delivered or select(2) or poll(2) return a readability event
     * because the connection might have  been  removed by an
     * asynchronous network error or another thread before accept()
     * is called.  If this happens then the call will block waiting
     * for the next connection to arrive.  To ensure that accept() never
     * blocks, the passed socket sockfd needs to have the O_NONBLOCK flag
     * set (see socket(7)).
     */

#ifdef HAVE_PPOLL
    struct pollfd fds;
    fds.fd = _fd;
#ifdef POLLRDHUP
    fds.events = POLLIN | POLLRDHUP;
#else
    fds.events = POLLIN;
#endif
#else
    fd_set fds,efds;
    FD_ZERO(&fds);
    FD_ZERO(&efds);
#endif

    sigset_t sigmask;
    // get current signal mask
    pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
    // unblock SIGUSR1 in pselect/ppoll
    sigdelset(&sigmask,SIGUSR1);

    int newfd;

    switch (_sockdomain) {
    case AF_INET:
        {
            struct sockaddr_in tmpaddr = sockaddr_in();
            socklen_t slen = sizeof(tmpaddr);

            // keep accepting until a valid connection
            // throw IOException of EINTR if a signal, such as SIGUSR1, is received

            // A high volume server could get fancy here and keep doing accept
            // until EAGAIN and return a list of sockets.
            for (;;) {

#ifdef HAVE_PPOLL
                if (::ppoll(&fds,1,NULL,&sigmask) < 0) {
                    throw IOException("ServerSocket: " + _localaddr->toAddressString(),"ppoll",errno);
                }
                if (fds.revents & POLLERR)
                    throw IOException("ServerSocket: " + _localaddr->toAddressString(),"accept POLLERR",errno);
#ifdef POLLRDHUP
                if (fds.revents & (POLLHUP | POLLRDHUP))
#else
                if (fds.revents & (POLLHUP)) 
#endif
                    NLOG(("ServerSocket %s: accept: POLLHUP",_localaddr->toAddressString().c_str()));

                if (!fds.revents & POLLIN) continue;
#else
                assert(_fd >= 0 && _fd < FD_SETSIZE);     // FD_SETSIZE=1024
                FD_SET(_fd,&fds);
                FD_SET(_fd,&efds);
                if (::pselect(_fd+1,&fds,NULL,&efds,NULL,&sigmask) < 0)
                    throw IOException("ServerSocket: " + _localaddr->toAddressString(),"pselect",errno);

                if (FD_ISSET(_fd,&efds))
                    throw IOException("ServerSocket: " + _localaddr->toAddressString(),"accept pselect exception",errno);
#endif

                if ((newfd = ::accept(_fd,(struct sockaddr*)&tmpaddr,&slen)) < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED)
                            continue;
                    throw IOException("ServerSocket: " + _localaddr->toAddressString(),"accept",errno);
                }
                return new Socket(newfd,Inet4SocketAddress(&tmpaddr));
            }
        }
        break;
    case AF_UNIX:
        {
            struct sockaddr_un tmpaddr = sockaddr_un();
            socklen_t slen = sizeof(tmpaddr);
            int newfd;

#ifdef HAVE_PPOLL
            if (::ppoll(&fds,1,NULL,&sigmask) < 0) {
                throw IOException("ServerSocket: " + _localaddr->toAddressString(),"ppoll",errno);
            }
            if (fds.revents & POLLERR)
                throw IOException("ServerSocket: " + _localaddr->toAddressString(),"accept POLLERR",errno);
#ifdef POLLRDHUP
            if (fds.revents & (POLLHUP | POLLRDHUP))
#else
            if (fds.revents & (POLLHUP)) 
#endif
                NLOG(("ServerSocket %s: POLLHUP",_localaddr->toAddressString().c_str()));

#else

            assert(_fd >= 0 && _fd < FD_SETSIZE);     // FD_SETSIZE=1024
            FD_SET(_fd,&fds);
            FD_SET(_fd,&efds);

            if (::pselect(_fd+1,&fds,NULL,&efds,NULL,&sigmask) < 0)
                throw IOException("ServerSocket: " + _localaddr->toAddressString(),"pselect",errno);

            if (FD_ISSET(_fd,&efds))
                throw IOException("ServerSocket: " + _localaddr->toAddressString(),"accept pselect exception",errno);
#endif

            if ((newfd = ::accept(_fd,(struct sockaddr*)&tmpaddr,&slen)) < 0)
                throw IOException("ServerSocket: " + _localaddr->toAddressString(),"accept",errno);

            /* An accept on a AF_UNIX socket does not return much info in
             * the sockaddr, just the family field (slen=2). The sun_path
             * portion is all zeroes. The same goes for the results of
             * getpeername on the new socket, so you have no information
             * about the remote end.  A AF_UNIX ServerSocket therefore has
             * no information about the remote end, except that it
             * successfully connected.
             *
             * getpeername after a connect gives address info, but getsockname doesn't.
             * getsockname does give address info after a bind.
             */
            VLOG(("AF_UNIX::accept, slen=") << slen);
            return new Socket(newfd,UnixSocketAddress(&tmpaddr));
        }
        break;
    }
    return 0;
}

void SocketImpl::getLocalAddr() throw(IOException)
{
    SocketAddress* newaddr;
    if (_sockdomain == AF_INET) {
        struct sockaddr_in tmpaddr;
        socklen_t slen = sizeof(tmpaddr);
        if (::getsockname(_fd,(struct sockaddr*)&tmpaddr,&slen) < 0)
            throw IOException("Socket","getsockname",errno);
        newaddr = new Inet4SocketAddress(&tmpaddr);
    }
    else {
        struct sockaddr_un tmpaddr = sockaddr_un();
        socklen_t slen = sizeof(tmpaddr);
        if (::getsockname(_fd,(struct sockaddr*)&tmpaddr,&slen) < 0)
            throw IOException("Socket","getsockname",errno);
        newaddr = new UnixSocketAddress(&tmpaddr);
        VLOG(("getsockname, slen=") << slen
             << " sizeof=" << sizeof(tmpaddr)
             << " addr=" << newaddr->toString());
    }
    delete _localaddr;
    _localaddr = newaddr;
}

void SocketImpl::getRemoteAddr() throw(IOException)
{
    SocketAddress* newaddr;
    if (_sockdomain == AF_INET) {
        struct sockaddr_in tmpaddr;
        socklen_t slen = sizeof(tmpaddr);
        if (::getpeername(_fd,(struct sockaddr*)&tmpaddr,&slen) < 0)
            throw IOException("Socket","getpeername",errno);
        newaddr = new Inet4SocketAddress(&tmpaddr);
    }
    else {
        struct sockaddr_un tmpaddr = sockaddr_un();
        socklen_t slen = sizeof(tmpaddr);
        if (::getpeername(_fd,(struct sockaddr*)&tmpaddr,&slen) < 0)
            throw IOException("Socket","getpeername",errno);
        newaddr = new UnixSocketAddress(&tmpaddr);
        VLOG(("getpeername, slen=") << slen
             << " sizeof=" << sizeof(tmpaddr)
             << " addr=" << newaddr->toString());
    }
    delete _remoteaddr;
    _remoteaddr = newaddr;
}

void SocketImpl::setNonBlocking(bool val) throw(IOException)
{
    int flags;
    /* get current flags */
    if ((flags = ::fcntl(_fd, F_GETFL, 0)) < 0)
    {
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "fcntl(...,F_GETFL,...)",ierr);
    }
    if (val)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;

    if (::fcntl(_fd, F_SETFL, flags) < 0)
    {
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "fcntl(...,F_SETFL,O_NONBLOCK)",ierr);
    }
    ILOG(("%s: setNonBlocking(%s)",_localaddr->toAddressString().c_str(),
          (val ? "true" : "false")));
}

bool SocketImpl::isNonBlocking() const throw(IOException)
{
    int flags;
    if ((flags = ::fcntl(_fd, F_GETFL, 0)) < 0) {
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "fcntl(...,F_GETFL,...)",ierr);
    }
    return (flags & O_NONBLOCK) != 0;
}

void SocketImpl::receive(DatagramPacketBase& packet) throw(IOException)
{
    if (_hasTimeout) {

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

        // get the existing signal mask
        sigset_t sigmask;
        pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
        // unblock SIGUSR1 in pselect/ppoll
        sigdelset(&sigmask,SIGUSR1);

        int pres;

#ifdef HAVE_PPOLL
        pres = ::ppoll(&fds,1,&_timeout,&sigmask);
        if (pres < 0)
            throw IOException(_localaddr->toAddressString(),"ppoll",errno);
        if (pres == 0)
            throw IOTimeoutException(_localaddr->toAddressString(),"receive");

        if (fds.revents & POLLERR)
            throw IOException(_localaddr->toAddressString(),"receive",errno);

#ifdef POLLRDHUP
        if (fds.revents & (POLLHUP | POLLRDHUP))
#else
        if (fds.revents & (POLLHUP))
#endif
        {
            ILOG(("%s: POLLHUP",_localaddr->toAddressString().c_str()));
            packet.setLength(0);
            return;
        }
#else
        if ((pres = ::pselect(_fd+1,&fdset,0,0,&_timeout,&sigmask)) < 0)
            throw IOException(_localaddr->toAddressString(),"pselect",errno);
        if (pres == 0)
            throw IOTimeoutException(_localaddr->toAddressString(),"receive");
#endif
    }
    ssize_t res;
    socklen_t slen = packet.getSockAddrLen();
    if ((res = ::recvfrom(_fd,packet.getDataVoidPtr(), packet.getMaxLength(),0,
                          packet.getSockAddrPtr(),&slen)) <= 0) {
        if (res < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                packet.setLength(0);
                return;
            }
            int ierr = errno;  // Inet4SocketAddress::toString changes errno
            throw IOException(_localaddr->toAddressString(),"receive",ierr);
        }
        else
            throw EOFException(_localaddr->toAddressString(), "receive");
    }
    packet.setLength(res);
}

void SocketImpl::receive(DatagramPacketBase& packet, Inet4PacketInfo& info, int flags) throw(IOException)
{
    if (_hasTimeout) {

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

        // get the existing signal mask
        sigset_t sigmask;
        pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
        // unblock SIGUSR1 in pselect/ppoll
        sigdelset(&sigmask,SIGUSR1);

        int pres;

#ifdef HAVE_PPOLL
        pres = ::ppoll(&fds,1,&_timeout,&sigmask);
        if (pres < 0)
            throw IOException(_localaddr->toAddressString(),"ppoll",errno);
        if (pres == 0)
            throw IOTimeoutException(_localaddr->toAddressString(),"receive");

        if (fds.revents & POLLERR)
            throw IOException(_localaddr->toAddressString(),"receive",errno);

#ifdef POLLRDHUP
        if (fds.revents & (POLLHUP | POLLRDHUP))
#else
        if (fds.revents & (POLLHUP))
#endif
        {
            ILOG(("%s: POLLHUP",_localaddr->toAddressString().c_str()));
            packet.setLength(0);
            return;
        }
#else
        if ((pres = ::pselect(_fd+1,&fdset,0,0,&_timeout,&sigmask)) < 0) {
            int ierr = errno;  // Inet4SocketAddress::toString changes errno
            throw IOException(_localaddr->toAddressString(),"pselect",ierr);
        }
        if (pres == 0)
            throw IOTimeoutException(_localaddr->toAddressString(),"receive");
#endif
    }

    struct msghdr mhdr = msghdr();

    mhdr.msg_name = packet.getSockAddrPtr();
    mhdr.msg_namelen = packet.getSockAddrLen();

    struct iovec iovec[1];
    iovec[0].iov_base = packet.getDataVoidPtr();
    iovec[0].iov_len = packet.getMaxLength();

    mhdr.msg_iov = iovec;
    mhdr.msg_iovlen = sizeof(iovec) / sizeof(iovec[0]);

    /* man cmsg */
    char cmsgbuf[CMSG_SPACE(sizeof(struct in_pktinfo))];
    mhdr.msg_control = cmsgbuf;
    mhdr.msg_controllen = sizeof(cmsgbuf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&mhdr);
    cmsg->cmsg_level = IPPROTO_IP;
    cmsg->cmsg_type = IP_PKTINFO;
    cmsg->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));

    // mhdr.msg_controllen = cmsg->cmsg_len;
    mhdr.msg_flags = 0;

    bool prevPktInfo = getPktInfo();
    if (! prevPktInfo) setPktInfo(true);

    ssize_t res;
    if ((res = ::recvmsg(_fd,&mhdr,flags)) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            packet.setLength(0);
            if (! prevPktInfo) setPktInfo(false);
            return;
        }
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),"receive",ierr);
    }
    packet.setLength(res);
    if (! prevPktInfo) setPktInfo(false);

    for (cmsg = CMSG_FIRSTHDR(&mhdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&mhdr,cmsg)) {
        if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
            struct in_pktinfo * pktinfoptr = (struct in_pktinfo *) CMSG_DATA(cmsg);

            info.setLocalAddress(Inet4Address(&pktinfoptr->ipi_spec_dst));
            info.setDestinationAddress(Inet4Address(&pktinfoptr->ipi_addr));
            struct ifreq ifreq;
            ifreq.ifr_ifindex = pktinfoptr->ipi_ifindex;
            // cerr << "index=" << pktinfoptr->ipi_ifindex << endl;
            if (ioctl(getFd(),SIOCGIFNAME,&ifreq) < 0) {
                // throw IOException("Socket","ioctl(,SIOCGIFNAME,)",errno);
                IOException e("Socket","ioctl(,SIOCGIFNAME,)",errno);
                WLOG(("%s, ifindex=%d",e.what(),pktinfoptr->ipi_ifindex));
                info.setInterface(getInterface());
            } else {

                info.setInterface(getInterface(ifreq.ifr_name));
            }
            {
                static LogContext lp(LOG_DEBUG);
                if (lp.active())
                {
                    const Inet4NetworkInterface& iface = info.getInterface();
                    LogMessage msg;
                    msg << "SocketImpl::receive() setting packet info: "
                        << "local=" << info.getLocalAddress().getHostAddress()
                        << "; dest="
                        << info.getDestinationAddress().getHostAddress()
                        << "; if=(" << iface.getName()
                        << "," << iface.getIndex()
                        << "," << iface.getAddress().getHostAddress()
                        << ")";
                    lp.log(msg);
                }
            }
            break;
       }
   }
}

size_t SocketImpl::recv(void* buf, size_t len, int flags)
    throw(IOException)
{
    if (_hasTimeout) {
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

        // get the existing signal mask
        sigset_t sigmask;
        pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
        // unblock SIGUSR1 in pselect/ppoll
        sigdelset(&sigmask,SIGUSR1);

        int pres;

#ifdef HAVE_PPOLL
        pres = ::ppoll(&fds,1,&_timeout,&sigmask);
        if (pres < 0)
            throw IOException(_localaddr->toAddressString(),"ppoll",errno);
        if (pres == 0)
            throw IOTimeoutException(_localaddr->toAddressString(),"recv");

        if (fds.revents & POLLERR)
            throw IOException(_localaddr->toAddressString(),"recv",errno);

#ifdef POLLRDHUP
        if (fds.revents & (POLLHUP | POLLRDHUP))
#else
        if (fds.revents & (POLLHUP))
#endif
        {
            ILOG(("%s: POLLHUP",_localaddr->toAddressString().c_str()));
            return 0;
        }
#else
        if ((pres = ::pselect(_fd+1,&fdset,0,0,&_timeout,&sigmask)) < 0)
            throw IOException(_localaddr->toAddressString(),"pselect",errno);
        if (pres == 0) 
            throw IOTimeoutException(_localaddr->toAddressString(),"recv");
#endif
    }
    ssize_t res;
    if ((res = ::recv(_fd,buf,len,flags)) <= 0) {
        if (res < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            int ierr = errno;  // Inet4SocketAddress::toString changes errno
            throw IOException(_localaddr->toAddressString(),"recv",ierr);
        }
        else throw EOFException(_localaddr->toAddressString(), "recv");
    }
    return res;
}

size_t SocketImpl::recvfrom(void* buf, size_t len, int flags,
                            SocketAddress& from) throw(IOException)
{
    if (_hasTimeout) {

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

        // get the existing signal mask
        sigset_t sigmask;
        pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
        // unblock SIGUSR1 in pselect/ppoll
        sigdelset(&sigmask,SIGUSR1);

        int pres;

#ifdef HAVE_PPOLL
        pres = ::ppoll(&fds,1,&_timeout,&sigmask);
        if (pres < 0)
            throw IOException(_localaddr->toAddressString(),"ppoll",errno);
        if (pres == 0)
            throw IOTimeoutException(_localaddr->toAddressString(),"recvfrom");

        if (fds.revents & POLLERR)
            throw IOException(_localaddr->toAddressString(),"recvfrom",errno);

#ifdef POLLRDHUP
        if (fds.revents & (POLLHUP | POLLRDHUP))
#else
        if (fds.revents & (POLLHUP))
#endif
        {
            ILOG(("%s: POLLHUP",_localaddr->toAddressString().c_str()));
            return 0;
        }
#else
        if ((pres = ::pselect(_fd+1,&fdset,0,0,&_timeout,&sigmask)) < 0)
            throw IOException(_localaddr->toAddressString(),"pselect",errno);
        if (pres == 0) 
            throw IOTimeoutException(_localaddr->toAddressString(),"recvfrom");
#endif
    }
    ssize_t res;
    socklen_t slen = from.getSockAddrLen();
    if ((res = ::recvfrom(_fd,buf,len,flags,
                          from.getSockAddrPtr(),&slen)) <= 0) {
        if (res < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            int ierr = errno;  // Inet4SocketAddress::toString changes errno
            throw IOException(from.toAddressString(),"recvfrom",ierr);
        }
        else throw EOFException(from.toAddressString(), "recvfrom");
    }
    return res;
}

void SocketImpl::send(const DatagramPacketBase& packet,int flags) throw(IOException)
{
    int res;

    VLOG(("sending packet, length=") << packet.getLength()
         << " slen=" << packet.getSockAddrLen()
         << " to " << packet.getSocketAddress().toAddressString());

    res = ::sendto(_fd, packet.getConstDataVoidPtr(), packet.getLength(), flags,
                   packet.getConstSockAddrPtr(), packet.getSockAddrLen());
    if (res < 0) {
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(packet.getSocketAddress().toAddressString(),"send",ierr);
    }
}

size_t SocketImpl::send(const void* buf, size_t len, int flags)
    throw(IOException)
{
    ssize_t res;
    if ((res = ::send(_fd,buf,len,flags)) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        int ierr = errno;  // Inet4SocketAddress::toString changes errno
        throw IOException(_remoteaddr->toAddressString(),"send",ierr);
    }
    return res;
}

size_t SocketImpl::send(const struct iovec* iov, int iovcnt, int flags)
    throw(IOException)
{
    ssize_t res;
    struct msghdr mhdr = msghdr();
    mhdr.msg_iov = const_cast<struct iovec*>(iov);
    mhdr.msg_iovlen = iovcnt;
    if ((res = ::sendmsg(_fd,&mhdr,flags)) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        int ierr = errno;  // Inet4SocketAddress::toString changes errno
        throw IOException(_remoteaddr->toAddressString(),"send",ierr);
    }
    return res;
}

size_t SocketImpl::sendto(const void* buf, size_t len, int flags,
                          const SocketAddress& to) throw(IOException)
{
    ssize_t res;
    if ((res = ::sendto(_fd,buf,len,flags,
                        to.getConstSockAddrPtr(),to.getSockAddrLen())) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        int ierr = errno;  // Inet4SocketAddress::toString changes errno
        throw IOException(to.toAddressString(), "sendto",ierr);
    }
    return res;
}

size_t SocketImpl::sendto(const struct iovec* iov, int iovcnt, int flags,
                          const SocketAddress& to) throw(IOException)
{
    ssize_t res;
    struct msghdr mhdr = msghdr();

    mhdr.msg_name = const_cast<void*>((const void*)to.getConstSockAddrPtr());
    mhdr.msg_namelen = to.getSockAddrLen();
    mhdr.msg_iov = const_cast<struct iovec*>(iov);
    mhdr.msg_iovlen = iovcnt;
    if ((res = ::sendmsg(_fd,&mhdr,flags)) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(_remoteaddr->toAddressString(),"send",ierr);
    }
    return res;
}

void SocketImpl::sendall(const void* buf, size_t len, int flags)
    throw(IOException)
{
    const char* cbuf = (const char*)buf;
    const char* eob = cbuf + len;

    while (cbuf < eob) {
        ssize_t res;
        if ((res = ::send(_fd,cbuf,len,flags)) < 0) {
            // if (errno == EAGAIN || errno == EWOULDBLOCK) sleep?
            int ierr = errno;   // Inet4SocketAddress::toString changes errno
            throw IOException(_remoteaddr->toAddressString(),"send",ierr);
        }
        cbuf += res;
        len -= res;
    }
}

void SocketImpl::setReceiveBufferSize(int size) throw(IOException) {
    if (::setsockopt(_fd, SOL_SOCKET, SO_RCVBUF,
                     (void *)&size, sizeof(size)) < 0)  {
        int ierr = errno;   // Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),"setsockopt SO_RCVBUF",ierr);
    }
}

int SocketImpl::getReceiveBufferSize() throw(IOException) {
    int size;
    socklen_t reqsize = sizeof(int);
    if (::getsockopt(_fd, SOL_SOCKET, SO_RCVBUF,
                     (void *)&size, &reqsize) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),"getsockopt SO_RCVBUF",ierr);
    }
    VLOG(("SocketImpl::getReceiveBufferSize()=") << size);
    return size;
}

void SocketImpl::setSendBufferSize(int size) throw(IOException) {
    if (::setsockopt(_fd, SOL_SOCKET, SO_SNDBUF,
                     (void *)&size, sizeof(size)) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),"setsockopt SO_SNDBUF",ierr);
    }
}

int SocketImpl::getSendBufferSize() throw(IOException) {
    int size;
    socklen_t reqsize = sizeof(int);
    if (::getsockopt(_fd, SOL_SOCKET, SO_SNDBUF,
                     (void *)&size, &reqsize) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),"getsockopt SO_SNDBUF",ierr);
    }
    VLOG(("SocketImpl::getSendBufferSize()=") << size);
    return size;
}

void SocketImpl::setTcpNoDelay(bool val) throw(IOException)
{
    if (getDomain() != AF_INET) return;
    int opt = val ? 1 : 0;
    socklen_t len = sizeof(opt);
    if (opt) ILOG(("%s: setting TCP_NODELAY",_localaddr->toAddressString().c_str()));
    if (setsockopt(_fd,SOL_TCP,TCP_NODELAY,(char *)&opt,len) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),"setsockopt TCP_NODELAY",ierr);
    }
}

bool SocketImpl::getTcpNoDelay() throw(IOException)
{
    if (getDomain() != AF_INET) return false;
    int opt = 0;
    socklen_t len = sizeof(opt);
    if (getsockopt(_fd,SOL_TCP,TCP_NODELAY,(char *)&opt,&len) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),"setsockopt TCP_NODELAY",ierr);
    }
    return opt != 0;
}

void SocketImpl::setKeepAlive(bool val) throw(IOException)
{
    if (getDomain() != AF_INET) return;
    int opt = val ? 1 : 0;
    socklen_t len = sizeof(opt);
    // man 7 socket
    if (setsockopt(_fd,SOL_SOCKET,SO_KEEPALIVE,(char *)&opt,len) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),"setsockopt SO_KEEPALIVE",ierr);
    }
}

bool SocketImpl::getKeepAlive() const throw(IOException)
{
    if (getDomain() != AF_INET) return false;
    int opt = 0;
    socklen_t len = sizeof(opt);
    if (getsockopt(_fd,SOL_SOCKET,SO_KEEPALIVE,(char *)&opt,&len) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),"setsockopt TCP_NODELAY",ierr);
    }
    return opt != 0;
}

void SocketImpl::setKeepAliveIdleSecs(int val) throw(IOException)
{
    if (getDomain() != AF_INET) return;
    socklen_t len = sizeof(val);
    /* man 7 tcp:
        tcp_keepalive_time
              The  number of seconds a connection needs to be idle before TCP begins send-
              ing out keep-alive probes.  Keep-alives are only sent when the  SO_KEEPALIVE
              socket  option is enabled.  The default value is 7200 seconds (2 hours).  An
              idle connection is terminated after approximately an additional  11  minutes
              (9 probes an interval of 75 seconds apart) when keep-alive is enabled.

              Note that underlying connection tracking mechanisms and application timeouts
              may be much shorter.
    */

    if (!getKeepAlive()) setKeepAlive(true);
    if (setsockopt(_fd,SOL_TCP,TCP_KEEPIDLE,(char *)&val,len) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "setsockopt TCP_KEEPIDLE", ierr);
    }
}

int SocketImpl::getKeepAliveIdleSecs() const throw(IOException)
{
    if (getDomain() != AF_INET) return 0;
    int val = 0;
    socklen_t len = sizeof(val);
    if (getsockopt(_fd,SOL_TCP,TCP_KEEPIDLE,(char *)&val,&len) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "setsockopt TCP_KEEPIDLE", ierr);
    }
    return val;
}

int SocketImpl::getInQueueSize() const throw(IOException)
{
    if (getDomain() != AF_INET) return 0;
    int val = 0;
    if (::ioctl(_fd,SIOCINQ,&val) < 0) {// man 7 tcp
        int ierr = errno;	// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "getInQueueSize: ioctl SIOCINQ", ierr);
    }
    return val;
}

int SocketImpl::getOutQueueSize() const throw(IOException)
{
    if (getDomain() != AF_INET) return 0;
    int val = 0;
    if (::ioctl(_fd,SIOCOUTQ,&val) < 0) {// man 7 tcp
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "getOutQueueSize: ioctl SIOCOUTQ", ierr);
    }
    return val;
}

#define JOIN_ALL_INTERFACES
#ifdef JOIN_ALL_INTERFACES
void SocketImpl::joinGroup(Inet4Address groupAddr) throw(IOException)
{
    list<Inet4NetworkInterface> ifcs = getInterfaces();
    // join on all interfaces
    int njoined = 0;
    for (list<Inet4NetworkInterface>::const_iterator ii = ifcs.begin();
         ii != ifcs.end(); ++ii)
    {
        Inet4NetworkInterface ifc = *ii;
        if (ifc.getFlags() & IFF_BROADCAST &&
            ifc.getFlags() & (IFF_MULTICAST |  IFF_LOOPBACK))
        {
            // On systems with multi-homed interfaces like ISS, the bind to
            // the aliased address will fail with EADDRINUSE.  So report
            // those errors but continue to join on the other interfaces.
            try {
                joinGroup(groupAddr, ifc);
                ++njoined;
            }
            catch (const IOException& ioe)
            {
                if (ioe.getErrno() == EADDRINUSE)
                {
                    ELOG(("continuing past address in use error: ")
                         << ioe.toString());
                }
                else
                {
                    throw ioe;
                }
            }
        }
    }
    // If none of the interfaces succeeded, then that's really a problem.
    if (ifcs.size() > 0 && njoined == 0)
    {
        throw IOException(groupAddr.getHostAddress(),
                          "joinGroup()",
                          "multicast join failed on all interfaces");
    }
}
#else
void SocketImpl::joinGroup(Inet4Address groupAddr) throw(IOException)
{
    struct ip_mreqn mreq;
    mreq.imr_multiaddr = groupAddr.getInAddr();
    mreq.imr_address.s_addr = INADDR_ANY;
    mreq.imr_ifindex = 0;

    if (::setsockopt(_fd, SOL_IP, IP_ADD_MEMBERSHIP,
                     (void *)&mreq, sizeof(mreq)) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(groupAddr.getHostAddress(),
                          "setsockopt IP_ADD_MEMBERSHIP",ierr);
    }
}

#endif

void
SocketImpl::
joinGroup(Inet4Address groupAddr,
          const Inet4NetworkInterface & iface) throw(IOException)
{
    struct ip_mreqn mreq;
    mreq.imr_multiaddr = groupAddr.getInAddr();
    mreq.imr_address = iface.getAddress().getInAddr();
    mreq.imr_ifindex = iface.getIndex();
    VLOG(("joining group, maddr=") << groupAddr.getHostAddress()
         << " iface=" << iface.getAddress().getHostAddress()
         << " index=" << iface.getIndex()
         << " iface name=" << iface.getName());

    if (::setsockopt(_fd, SOL_IP, IP_ADD_MEMBERSHIP,
                     (void *)&mreq, sizeof(mreq)) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(groupAddr.getHostAddress(),
                          "setsockopt IP_ADD_MEMBERSHIP",ierr);
    }
}

void SocketImpl::leaveGroup(Inet4Address groupAddr) throw(IOException)
{
    list<Inet4NetworkInterface> ifcs = getInterfaces();
    // leave on all interfaces
    for (list<Inet4NetworkInterface>::const_iterator ii = ifcs.begin();
         ii != ifcs.end(); ++ii)
    {
        leaveGroup(groupAddr,*ii);
    }
}

void
SocketImpl::
leaveGroup(Inet4Address groupAddr,const Inet4NetworkInterface& iface)
    throw(IOException)
{
    struct ip_mreqn mreq;
    mreq.imr_multiaddr = groupAddr.getInAddr();
    mreq.imr_address = iface.getAddress().getInAddr();
    mreq.imr_ifindex = iface.getIndex();

    if (::setsockopt(_fd, SOL_IP, IP_DROP_MEMBERSHIP,
                     (void *)&mreq, sizeof(mreq)) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(groupAddr.getHostAddress(),
                          "setsockopt IP_DROP_MEMBERSHIP",ierr);
    }
}

void
SocketImpl::
setInterface(Inet4Address maddr, const Inet4NetworkInterface& iface)
    throw(IOException)
{
    struct ip_mreqn mreq;
    mreq.imr_multiaddr = maddr.getInAddr();
    mreq.imr_address = iface.getAddress().getInAddr();
    mreq.imr_ifindex = iface.getIndex();
    VLOG(("setInterface, maddr=") << maddr.getHostAddress()
         << " iface=" << iface.getAddress().getHostAddress()
         << " index=" << iface.getIndex()
         << " iface name=" << iface.getName());

    if (::setsockopt(_fd, SOL_IP, IP_MULTICAST_IF,
                     (void *)&mreq, sizeof(mreq)) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "setsockopt IP_MULTICAST_IF",ierr);
    }
}

void SocketImpl::setInterface(Inet4Address maddr) throw(IOException)
{
    struct ip_mreqn mreq;
    mreq.imr_multiaddr = maddr.getInAddr();
    mreq.imr_address.s_addr = INADDR_ANY;
    mreq.imr_ifindex = 0;

    if (::setsockopt(_fd, SOL_IP, IP_MULTICAST_IF,
                     (void *)&mreq, sizeof(mreq)) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "setsockopt IP_MULTICAST_IF",ierr);
    }
}

Inet4NetworkInterface SocketImpl::findInterface(const Inet4Address& iaddr) const
    throw(IOException)
{
    list<Inet4NetworkInterface> ifcs = getInterfaces();
    Inet4NetworkInterface mtch;
    int bits = 0;
    for (list<Inet4NetworkInterface>::const_iterator ii = ifcs.begin();
         ii != ifcs.end(); ++ii)
    {
        Inet4NetworkInterface iface = *ii;
        if (iaddr == Inet4Address(INADDR_ANY)) return iface;
        Inet4Address addr = ii->getAddress();
        int ib;
        if ((ib = iaddr.bitsMatch(addr)) > bits) {
            mtch = *ii;
            bits = ib;
        }
    }
    return mtch;
}

Inet4NetworkInterface SocketImpl::getInterface() const throw(IOException)
{
    struct ip_mreqn mreq = ip_mreqn();
    socklen_t reqsize = sizeof(mreq);
    if (::getsockopt(_fd, SOL_IP, IP_MULTICAST_IF,(void *)&mreq, &reqsize) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "getsockopt IP_MULTICAST_IF",ierr);
    }
    VLOG(("reqsize=") << reqsize);
    // If the returned size is 4, then the contents of imr_multiaddr appears
    // to be the local interface address.
    if (reqsize == 4) {

        VLOG(("Inet4NetworkInterface SocketImpl::getInterface(): iface index=")
             << mreq.imr_ifindex << " mcastaddr = "
             << Inet4Address(ntohl(mreq.imr_multiaddr.s_addr)).getHostAddress()
             << " address = "
             << Inet4Address(ntohl(mreq.imr_address.s_addr)).getHostAddress());
        Inet4Address addr(ntohl(mreq.imr_multiaddr.s_addr));
        list<Inet4NetworkInterface> ifaces = getInterfaces();
        list<Inet4NetworkInterface>::const_iterator ii = ifaces.begin();
        for ( ; ii != ifaces.end(); ++ii) {
            Inet4NetworkInterface iface = *ii;
            if (iface.getAddress() == addr) return iface;
        }
        return Inet4NetworkInterface("", addr, Inet4Address(INADDR_ANY),
                                     Inet4Address(INADDR_BROADCAST), 0, 0, 0);
    }
    VLOG(("Inet4NetworkInterface SocketImpl::getInterface(): iface index=")
         << mreq.imr_ifindex << " mcastaddr = "
         << Inet4Address(ntohl(mreq.imr_multiaddr.s_addr)).getHostAddress()
         << " address = "
         << Inet4Address(ntohl(mreq.imr_address.s_addr)).getHostAddress());

    if (mreq.imr_ifindex == 0)
        return Inet4NetworkInterface();

    struct ifreq ifreq;

    ifreq.ifr_ifindex = mreq.imr_ifindex;
    if (ioctl(getFd(),SIOCGIFNAME,&ifreq) < 0)
        throw IOException("Socket","ioctl(,SIOCGIFNAME,)",errno);
    VLOG(("Inet4NetworkInterface SocketImpl::getInterface(): iface name=")
         << ifreq.ifr_name);

    return getInterface(ifreq.ifr_name);
}

void SocketImpl::setTimeToLive(int val) throw(IOException)
{
    if (setsockopt(_fd, SOL_IP, IP_MULTICAST_TTL,
                   (void *)&val, sizeof(val)) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "setsockopt IP_MULTICAST_TTL",ierr);
    }
}

int SocketImpl::getTimeToLive() const throw(IOException)
{
    int val;
    socklen_t reqsize = sizeof(val);
    if (::getsockopt(_fd, SOL_IP, IP_MULTICAST_TTL,
                     (void *)&val, &reqsize) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "getsockopt IP_MULTICAST_TTL",ierr);
    }
    return val;
}

void SocketImpl::setMulticastLoop(bool val) throw(IOException)
{
    int loop = (val ? 1 : 0);
    if (setsockopt(_fd, SOL_IP, IP_MULTICAST_LOOP,
        (void*)&loop,sizeof(loop)) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "setsockopt IP_MULTICAST_LOOP",ierr);
    }
}

bool SocketImpl::getMulticastLoop() const throw(IOException)
{
    int val;
    socklen_t reqsize = sizeof(val);
    if (::getsockopt(_fd, SOL_IP, IP_MULTICAST_LOOP,
                     (void *)&val, &reqsize) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "getsockopt IP_MULTICAST_LOOP",ierr);
    }
    return val != 0;
}

void SocketImpl::setBroadcastEnable(bool val) throw(IOException)
{
    int bcast = val ? 1 : 0;
    if (::setsockopt(_fd, SOL_SOCKET, SO_BROADCAST,
                     (void *)&bcast, sizeof(bcast)) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "setsockopt SO_BROADCAST",ierr);
    }
}

bool SocketImpl::getBroadcastEnable() const throw(IOException)
{
    int bcast;
    socklen_t slen = sizeof(bcast);
    if (::getsockopt(_fd, SOL_SOCKET, SO_BROADCAST,
                     (void *)&bcast, &slen) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "getsockopt SO_BROADCAST",ierr);
    }
    return bcast != 0;
}

Inet4NetworkInterface
SocketImpl::
getInterface(const std::string& name) const throw(IOException)
{
    struct ifreq ifreq;
    strncpy(ifreq.ifr_name,name.c_str(),IF_NAMESIZE);

    // ioctls in /usr/include/bits/ioctls.h
    if (ioctl(getFd(),SIOCGIFADDR,&ifreq) < 0)
            throw IOException("Socket","ioctl(,SIOCGIFADDR,)",errno);
    Inet4SocketAddress saddr =
        Inet4SocketAddress((const sockaddr_in*)&ifreq.ifr_addr);
    Inet4Address addr = saddr.getInet4Address();

    if (ioctl(getFd(),SIOCGIFBRDADDR,&ifreq) < 0)
            throw IOException("Socket","ioctl(,SIOCGIFBRDADDR,)",errno);
    saddr = Inet4SocketAddress((const sockaddr_in*)&ifreq.ifr_broadaddr);
    Inet4Address baddr = saddr.getInet4Address();

    if (ioctl(getFd(),SIOCGIFNETMASK,&ifreq) < 0)
            throw IOException("Socket","ioctl(,SIOCGIFNETMASK,)",errno);
    saddr = Inet4SocketAddress((const sockaddr_in*)&ifreq.ifr_netmask);
    Inet4Address maddr = saddr.getInet4Address();

    if (ioctl(getFd(),SIOCGIFMTU,&ifreq) < 0)
            throw IOException("Socket","ioctl(,SIOCGIFMTU,)",errno);
    int mtu = ifreq.ifr_mtu;

    if (ioctl(getFd(),SIOCGIFINDEX,&ifreq) < 0)
            throw IOException("Socket","ioctl(,SIOCGIFINDEX,)",errno);
    int index = ifreq.ifr_ifindex;

    if (ioctl(getFd(),SIOCGIFFLAGS,&ifreq) < 0)
            throw IOException("Socket","ioctl(,SIOCGIFFLAGS,)",errno);
    short flags = ifreq.ifr_flags;

    VLOG(("addr=") << saddr.toString());
    return Inet4NetworkInterface(name,addr,baddr,maddr,mtu,index,flags);
}

list<Inet4NetworkInterface> SocketImpl::getInterfaces() const throw(IOException)
{
    list<Inet4NetworkInterface> interfaces;

// #define USE_IF_NAMEINDEX
#ifdef USE_IF_NAMEINDEX
    // if_nameindex returns all interfaces, whether there is an address
    // associated with it or not.  If the name of an index without
    // an associated address is passed to ioctl(fd,SIOCGIFADDR,&ifreq)
    // it will fail with EADDRNOTAVAIL: "Cannot assign requested address"
    //
    struct if_nameindex * ifptr0 = if_nameindex();
    if (!ifptr0) {
        int ierr = errno;
        throw IOException("Socket", "if_nameindex",ierr);
    }
    for (struct if_nameindex * ifptr = ifptr0 ; ifptr->if_index != 0; ifptr++) {
        Inet4NetworkInterface ifc = getInterface(ifptr->if_name);
        interfaces.push_back(ifc);
    }
    if_freenameindex(ifptr0);
#else

    // do "man netdevice" from linux for info on the SIOCGIFCONF ioctl
    struct ifconf ifcs;
    vector<char> ifcbuf;
    for (int bufsize=4096; ; bufsize += 1024) {
        ifcbuf.resize(bufsize);
        ifcs.ifc_buf = &ifcbuf.front();
        ifcs.ifc_len = bufsize;
        if (ioctl(getFd(),SIOCGIFCONF,&ifcs) < 0)
            throw IOException("Socket","ioctl(,SIOCGIFCONF,)",errno);
        if (ifcs.ifc_len != bufsize) break;
        VLOG(("bufsize=") << bufsize << " is not enough");
    }
    assert((ifcs.ifc_len % sizeof(struct ifreq)) == 0);
    int ninterfaces = ifcs.ifc_len / sizeof(struct ifreq);

    for (int i = 0; i < ninterfaces; i++) {
        VLOG(("name=") << ifcs.ifc_req[i].ifr_name);
        Inet4NetworkInterface ifc = getInterface(ifcs.ifc_req[i].ifr_name);
        interfaces.push_back(ifc);
    }
#endif
    return interfaces;
}

void SocketImpl::setPktInfo(bool val) throw(IOException)
{
    int opt = val ? 1 : 0;
    socklen_t len = sizeof(opt);
    if (::setsockopt(_fd, IPPROTO_IP,IP_PKTINFO,(char*)&opt,len) < 0) {
        int ierr = errno;// Inet4SocketAddress::toString changes errno
        throw IOException(_localaddr->toAddressString(),
                          "setsockopt IP_PKTINFO", ierr);
    }
    _pktInfo = val;
}

Socket::Socket(int domain) throw(IOException) :
    _impl(domain,SOCK_STREAM)
{
}

Socket::Socket(int fd, const SocketAddress& remoteaddr) throw(IOException) :
    _impl(fd,remoteaddr)
{
}

Socket::Socket(const std::string& dest,int port)
    throw(UnknownHostException,IOException) :
    _impl(AF_INET,SOCK_STREAM)
{
    // If an exception in the connect, close the file descriptor.
    // The destructor for _impl will be called on an exception.
    try {
        connect(dest,port);
    }
    catch(...) {
        close();
        throw;
    }
}

Socket::Socket(const Inet4Address& addr,int port)
    throw(IOException) :
    _impl(AF_INET,SOCK_STREAM)
{
    try {
        connect(addr,port);
    }
    catch(...) {
        close();
        throw;
    }
}

Socket::Socket(const SocketAddress& addr)
    throw(IOException) :
    _impl(addr.getFamily(),SOCK_STREAM)
{
    try {
        connect(addr);
    }
    catch(...) {
        close();
        throw;
    }
}

ServerSocket::ServerSocket(int port,int backlog)
    throw(IOException) :
    _impl(AF_INET,SOCK_STREAM)
{
    // it's generally wise to use non blocking ServerSockets. See 
    // comments in accept() method.
    try {
        _impl.bind(port);
    }
    catch(...) {
        close();
        throw;
    }
    _impl.setNonBlocking(true);
    _impl.setBacklog(backlog);
}

ServerSocket::ServerSocket(const Inet4Address& addr, int port,int backlog)
    throw(IOException) :
    _impl(AF_INET,SOCK_STREAM)
{
    // it's generally wise to use non blocking ServerSockets. See 
    // comments in accept() method.
    try {
        _impl.bind(addr,port);
    }
    catch(...) {
        close();
        throw;
    }
    _impl.setNonBlocking(true);
    _impl.setBacklog(backlog);
}

ServerSocket::ServerSocket(const SocketAddress& addr,int backlog)
    throw(IOException) :
    _impl(addr.getFamily(),SOCK_STREAM)
{
    // it's generally wise to use non blocking ServerSockets. See 
    // comments in accept() method.
    try {
        _impl.bind(addr);
    }
    catch(...) {
        close();
        throw;
    }
    _impl.setNonBlocking(true);
    _impl.setBacklog(backlog);
}

void ServerSocket::close() throw(IOException)
{
    _impl.close();
    if (getDomain() == AF_UNIX) {
        string path = getLocalSocketAddress().toString();
        if (path.substr(0,6) == "unix:/") path = path.substr(5);
        if (path != "null") {
            struct stat statbuf;
            if (::stat(path.c_str(),&statbuf) == 0 &&
                S_ISSOCK(statbuf.st_mode)) {
                DLOG(("unlinking: ") << path);
                ::unlink(path.c_str());
            }
        }
    }
}

DatagramSocket::DatagramSocket() throw(IOException) :
    _impl(AF_INET,SOCK_DGRAM)
{
}

DatagramSocket::DatagramSocket(int port) throw(IOException) :
    _impl(AF_INET,SOCK_DGRAM)
{
    try {
        _impl.bind(port);
    }
    catch(...) {
        close();
        throw;
    }
}

DatagramSocket::DatagramSocket(const Inet4Address& addr,int port)
    throw(IOException) :
    _impl(AF_INET,SOCK_DGRAM)
{
    try {
        _impl.bind(addr,port);
    }
    catch(...) {
        close();
        throw;
    }
}

DatagramSocket::DatagramSocket(const SocketAddress& addr)
    throw(IOException) :
    _impl(addr.getFamily(),SOCK_DGRAM)
{
    try {
        _impl.bind(addr);
    }
    catch(...) {
        close();
        throw;
    }
}

/* static */
vector<Socket*> Socket::createSocketPair(int type) throw(IOException)
{
    int fds[2];
    if (::socketpair(AF_UNIX,type,0,fds) < 0) 
        throw IOException("socketpair","create",errno);
    vector<Socket*> res;
    res.push_back(new Socket(fds[0],UnixSocketAddress("")));
    res.push_back(new Socket(fds[1],UnixSocketAddress("")));
    return res;
}
