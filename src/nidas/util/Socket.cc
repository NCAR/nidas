//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#include <nidas/util/Socket.h>
#include <nidas/util/Inet4SocketAddress.h>
#include <nidas/util/UnixSocketAddress.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/IOTimeoutException.h>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>

#include <sys/socket.h>
#include <sys/ioctl.h>  // ioctl(,SIOCGIFCONF,)
#include <net/if.h>     // ioctl(,SIOCGIFCONF,)
#include <sys/un.h>     // ioctl(,SIOCGIFCONF,)
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <linux/sockios.h>
                                                                                
#ifdef DEBUG
#endif
#include <iostream>

using namespace nidas::util;
using namespace std;

SocketImpl::SocketImpl(int domain,int type) throw(IOException):
    sockdomain(domain),socktype(type),
    localaddr(0),remoteaddr(0),
    fd(-1),backlog(10),reuseaddr(true),
    hasTimeout(false)
{
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    if ((fd = ::socket(sockdomain,socktype, 0)) < 0)
	throw IOException("Socket","open",errno);
    getLocalAddr();
    remoteaddr = localaddr->clone();	// not connected yet
}

SocketImpl::SocketImpl(int fda, const SocketAddress& raddr)
	throw(IOException) :
    sockdomain(raddr.getFamily()),// AF_* are equal to PF_* in sys/socket.h
    socktype(SOCK_STREAM),
    localaddr(0),
    remoteaddr(raddr.clone()),
    fd(fda),
    backlog(10),reuseaddr(true),
    hasTimeout(false)
{
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    getLocalAddr();
    // getRemoteAddr();
}

/* copy constructor */
SocketImpl::SocketImpl(const SocketImpl& x):
    sockdomain(x.sockdomain),socktype(x.socktype),
    localaddr(x.localaddr->clone()),remoteaddr(x.remoteaddr->clone()),
    fd(x.fd),
    backlog(x.backlog),reuseaddr(x.reuseaddr),
    hasTimeout(x.hasTimeout),timeout(x.timeout)
{
}

/* assignment operator */
SocketImpl& SocketImpl::operator =(const SocketImpl& rhs)
{
    if (this != &rhs) {
        sockdomain = rhs.sockdomain;
        socktype = rhs.socktype;
        delete localaddr;
        localaddr = rhs.localaddr->clone();
        delete remoteaddr;
        remoteaddr = rhs.remoteaddr->clone();
        fd = rhs.fd;
        backlog = rhs.backlog;
        reuseaddr = rhs.reuseaddr;
        hasTimeout = rhs.hasTimeout;
        timeout = rhs.timeout;
    }
    return *this;
}

SocketImpl::~SocketImpl()
{
    delete localaddr;
    delete remoteaddr;
}

void SocketImpl::setTimeout(int val)
{
    timeout.tv_sec = val / 1000;
    timeout.tv_usec = (val % 1000) * 1000;
    hasTimeout = val > 0;
    FD_ZERO(&fdset);
}

int SocketImpl::getTimeout() const
{
    return timeout.tv_sec * 1000 + timeout.tv_usec / 1000;
}

void SocketImpl::close() throw(IOException) 
{

#ifdef DEBUG
    cerr << "closing, local=" << getLocalSocketAddress().toString()
	 << " remote=" << getRemoteSocketAddress().toString() << endl;
#endif
    if (fd >= 0 && ::close(fd) < 0) 
    	throw IOException("Socket","close",errno);
    fd = -1;
    if (getDomain() == PF_UNIX) {
        string path = getLocalSocketAddress().toString();
        if (path.substr(0,5) == "unix:") path = path.substr(5);
        if (path != "null") {
            struct stat statbuf;
            if (::stat(path.c_str(),&statbuf) == 0 &&
                S_ISSOCK(statbuf.st_mode)) {
                cerr << "unlinking: " << path << endl;
                ::unlink(path.c_str());
            }
        }
    }
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
    if (fd < 0 && (fd = ::socket(sockdomain,socktype, 0)) < 0)
	throw IOException("Socket","open",errno);
    // works for both PF_UNIX, PF_INET
    if (::connect(fd,addr.getConstSockAddrPtr(),addr.getSockAddrLen()) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(addr.toString(),"connect",ierr);
    }
    // For INET sockets the local address isn't set until a connect,
    // so we do it now.
    // For UNIX sockets a getsockname() after a connect doesn't give
    // any local address info, so getLocalAddr doesn't do much.
    getLocalAddr();	
    getRemoteAddr();
}


void SocketImpl::bind(int port) throw(IOException)
{
    Inet4Address addr;	// default
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
    if (fd < 0 && (fd = ::socket(sockdomain,socktype, 0)) < 0)
	throw IOException("Socket","open",errno);
    int rval = reuseaddr ? 1 : 0;        /* flag for setsocketopt */
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
    	(void *)&rval, sizeof(rval)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(sockaddr.toString(),"setsockopt SO_REUSEADDR",ierr);
    }

    if (::bind(fd,sockaddr.getConstSockAddrPtr(),
    	sockaddr.getSockAddrLen()) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(sockaddr.toString(),"bind",ierr);
    }

    if (socktype == SOCK_STREAM) listen();

    // get actual local address
    getLocalAddr();
}

void SocketImpl::listen() throw(IOException)
{
    if (fd < 0 && (fd = ::socket(sockdomain,socktype, 0)) < 0)
	throw IOException("Socket","open",errno);
    if (::listen(fd,backlog) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"listen",ierr);
    }
}

Socket* SocketImpl::accept() throw(IOException)
{
    if (fd < 0 && (fd = ::socket(sockdomain,socktype, 0)) < 0)
	throw IOException("Socket","open",errno);
    switch (sockdomain) {
    case PF_INET:
	{

/* useful bit of wisdom from man 2 accept:
 * There  may  not always be a connection waiting after a SIGIO is delivered or select(2)
 * or poll(2) return a readability event because the connection might have  been  removed
 * by an asynchronous network error or another thread before accept() is called.  If this
 * happens then the call will block waiting for the next connection to arrive.  To ensure
 * that accept() never blocks, the passed socket sockfd needs to have the O_NONBLOCK flag
 * set (see socket(7)).
 * Also:
 * Linux  accept()  passes  already-pending  network errors on the new socket as an error
 * code from accept().  This behaviour differs from other BSD socket implementations. For
 * reliable  operation  the  application should detect the network errors defined for the
 * protocol after accept() and treat them like EAGAIN by  retrying.  In  case  of  TCP/IP
 * these  are ENETDOWN, EPROTO, ENOPROTOOPT, EHOSTDOWN, ENONET, EHOSTUNREACH, EOPNOTSUPP,
 * and ENETUNREACH.
 */
	    struct sockaddr_in tmpaddr;
	    socklen_t slen = sizeof(tmpaddr);
	    memset(&tmpaddr,0,slen);
	    int newfd;
	    if ((newfd = ::accept(fd,(struct sockaddr*)&tmpaddr,&slen)) < 0)
		    throw IOException("Socket","accept",errno);
	    return new Socket(newfd,Inet4SocketAddress(&tmpaddr));
	}
	break;
    case PF_UNIX:
	{
	    struct sockaddr_un tmpaddr;
	    socklen_t slen = sizeof(tmpaddr);
	    memset(&tmpaddr,0,slen);
	    int newfd;
	    if ((newfd = ::accept(fd,(struct sockaddr*)&tmpaddr,&slen)) < 0)
		    throw IOException("Socket","accept",errno);

	    /* An accept on a PF_UNIX socket does not return much info in
	     * the sockaddr, just the family field (slen=2). The sun_path
	     * portion is all zeroes. The same goes for the results of
	     * getpeername on the new socket, so you have no information
	     * about the remote end.  A PF_UNIX ServerSocket therefore has
	     * no information about the remote end, except that it
	     * successfully connected.
	     *
	     * getpeername after a connect gives address info, but getsockname doesn't.
	     * getsockname does give address info after a bind.
	     */
#ifdef DEBUG
	    cerr << "PF_UNIX::accept, slen=" << slen << endl;
#endif
	    return new Socket(newfd,UnixSocketAddress(&tmpaddr));
	}
	break;
    }
    return 0;
}

void SocketImpl::getLocalAddr() throw(IOException)
{
    SocketAddress* newaddr;
    if (sockdomain == PF_INET) {
	struct sockaddr_in tmpaddr;
	socklen_t slen = sizeof(tmpaddr);
	if (::getsockname(fd,(struct sockaddr*)&tmpaddr,&slen) < 0)
	    throw IOException("Socket","getsockname",errno);
	newaddr = new Inet4SocketAddress(&tmpaddr);
    }
    else {
	struct sockaddr_un tmpaddr;
	socklen_t slen = sizeof(tmpaddr);
	memset(&tmpaddr,0,slen);
	if (::getsockname(fd,(struct sockaddr*)&tmpaddr,&slen) < 0)
	    throw IOException("Socket","getsockname",errno);
	newaddr = new UnixSocketAddress(&tmpaddr);
#ifdef DEBUG
	cerr << "getsockname, slen=" << slen <<
		" sizeof=" << sizeof(tmpaddr) <<
		" addr=" << newaddr->toString() << endl;
#endif
    }
    delete localaddr;
    localaddr = newaddr;
}

void SocketImpl::getRemoteAddr() throw(IOException)
{
    SocketAddress* newaddr;
    if (sockdomain == PF_INET) {
	struct sockaddr_in tmpaddr;
	socklen_t slen = sizeof(tmpaddr);
	if (::getpeername(fd,(struct sockaddr*)&tmpaddr,&slen) < 0)
	    throw IOException("Socket","getpeername",errno);
	newaddr = new Inet4SocketAddress(&tmpaddr);
    }
    else {
	struct sockaddr_un tmpaddr;
	socklen_t slen = sizeof(tmpaddr);
	memset(&tmpaddr,0,slen);
	if (::getpeername(fd,(struct sockaddr*)&tmpaddr,&slen) < 0)
	    throw IOException("Socket","getpeername",errno);
	newaddr = new UnixSocketAddress(&tmpaddr);
#ifdef DEBUG
	cerr << "getpeername, slen=" << slen <<
		" sizeof=" << sizeof(tmpaddr) <<
		" addr=" << newaddr->toString() << endl;
#endif
    }
    delete remoteaddr;
    remoteaddr = newaddr;
}

void SocketImpl::setNonBlocking(bool val) throw(IOException)
{
    int flags;
    /* set io to non-blocking, so network jams don't hang us up */
    if ((flags = ::fcntl(fd, F_GETFL, 0)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
        throw IOException(localaddr->toString(),
		"fcntl(...,F_GETFL,...)",ierr);
    }
    if (val) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;

    if (::fcntl(fd, F_SETFL, flags) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
        throw IOException(localaddr->toString(),
		"fcntl(...,F_SETFL,O_NONBLOCK)",ierr);
    }
}

bool SocketImpl::isNonBlocking() const throw(IOException)
{
    int flags;
    /* set io to non-blocking, so network jams don't hang us up */
    if ((flags = ::fcntl(fd, F_GETFL, 0)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
        throw IOException(localaddr->toString(),
		"fcntl(...,F_GETFL,...)",ierr);
    }
    return (flags & O_NONBLOCK) != 0;
}

void SocketImpl::receive(DatagramPacketBase& packet) throw(IOException)
{
    int res;
    if (hasTimeout) {
	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);
	struct timeval tmpto = timeout;
	if ((res = ::select(fd+1,&fdset,0,0,&tmpto)) < 0) {
	    int ierr = errno;	// Inet4SocketAddress::toString changes errno
	    throw IOException(localaddr->toString(),"receive",ierr);
	}
	if (res == 0)
	    throw IOTimeoutException(localaddr->toString(),"receive");
    }

    socklen_t slen = packet.getSockAddrLen();
    if ((res = ::recvfrom(fd,packet.getDataVoidPtr(), packet.getMaxLength(),0,
    	packet.getSockAddrPtr(),&slen)) <= 0) {
	if (res < 0) {
	    int ierr = errno;	// Inet4SocketAddress::toString changes errno
	    throw IOException(localaddr->toString(),"receive",ierr);
	}
	else throw EOFException(localaddr->toString(), "receive");
    }
    packet.setLength(res);
}

void SocketImpl::send(const DatagramPacketBase& packet,int flags) throw(IOException)
{
    int res;

#ifdef DEBUG
    cerr << "sending packet, length=" << packet.getLength()
	 << " slen=" << packet.getSockAddrLen() << endl;
#endif

    if ((res = ::sendto(fd,packet.getConstDataVoidPtr(),packet.getLength(),flags,
    	packet.getConstSockAddrPtr(),packet.getSockAddrLen())) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(packet.getSocketAddress().toString(),"send",ierr);
    }
}


size_t SocketImpl::recv(void* buf, size_t len, int flags)
	throw(IOException)
{
    ssize_t res;
    if (hasTimeout) {
	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);
	struct timeval tmpto = timeout;
	if ((res = ::select(fd+1,&fdset,0,0,&tmpto)) < 0) {
	    int ierr = errno;	// Inet4SocketAddress::toString changes errno
	    throw IOException(localaddr->toString(),"receive",ierr);
	}
	if (res == 0) 
	    throw IOTimeoutException(localaddr->toString(),"receive");
    }
    if ((res = ::recv(fd,buf,len,flags)) <= 0) {
	if (res < 0) {
	    int ierr = errno;	// Inet4SocketAddress::toString changes errno
	    throw IOException(localaddr->toString(),"recv",ierr);
	}
	else throw EOFException(localaddr->toString(), "recv");
    }
    return res;
}

size_t SocketImpl::recvfrom(void* buf, size_t len, int flags,
	SocketAddress& from) throw(IOException)
{
    ssize_t res;
    if (hasTimeout) {
	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);
	struct timeval tmpto = timeout;
	if ((res = ::select(fd+1,&fdset,0,0,&tmpto)) < 0) {
	    int ierr = errno;	// Inet4SocketAddress::toString changes errno
	    throw IOException(localaddr->toString(),"receive",ierr);
	}
	if (res == 0) 
	    throw IOTimeoutException(localaddr->toString(),"receive");
    }

    socklen_t slen = from.getSockAddrLen();
    if ((res = ::recvfrom(fd,buf,len,flags,
    	from.getSockAddrPtr(),&slen)) <= 0) {
	if (res < 0) {
	    int ierr = errno;	// Inet4SocketAddress::toString changes errno
	    throw IOException(from.toString(),"recvfrom",ierr);
	}
	else throw EOFException(from.toString(), "recvfrom");
    }
    return res;
}

size_t SocketImpl::send(const void* buf, size_t len, int flags)
	throw(IOException)
{
    ssize_t res;
    if ((res = ::send(fd,buf,len,flags)) < 0) {
	if (errno == EAGAIN) return 0;
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(remoteaddr->toString(),"send",ierr);
    }
    return res;
}

size_t SocketImpl::sendto(const void* buf, size_t len, int flags,
	const SocketAddress& to) throw(IOException)
{
    ssize_t res;
    if ((res = ::sendto(fd,buf,len,flags,
    	to.getConstSockAddrPtr(),to.getSockAddrLen())) < 0) {
	if (errno == EAGAIN) return 0;
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(to.toString(), "sendto",ierr);
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
	if ((res = ::send(fd,cbuf,len,flags)) < 0) {
	    // if (errno == EAGAIN) sleep?
	    int ierr = errno;	// Inet4SocketAddress::toString changes errno
	    throw IOException(remoteaddr->toString(),"send",ierr);
	}
	cbuf += res;
	len -= res;
    }
}

void SocketImpl::setReceiveBufferSize(int size) throw(IOException) {
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
    	(void *)&size, sizeof(size)) < 0)  {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"setsockopt SO_RCVBUF",ierr);
    }
}

int SocketImpl::getReceiveBufferSize() throw(IOException) {
    int size;
    socklen_t reqsize = sizeof(int);
    if (::getsockopt(fd, SOL_SOCKET, SO_RCVBUF,
    	(void *)&size, &reqsize) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"getsockopt SO_RCVBUF",ierr);
    }
#ifdef DEBUG
    cerr << "SocketImpl::getReceiveBufferSize()=" << size << endl;
#endif
    return size;
}

void SocketImpl::setSendBufferSize(int size) throw(IOException) {
    if (::setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
    	(void *)&size, sizeof(size)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"setsockopt SO_SNDBUF",ierr);
    }
}

int SocketImpl::getSendBufferSize() throw(IOException) {
    int size;
    socklen_t reqsize = sizeof(int);
    if (::getsockopt(fd, SOL_SOCKET, SO_SNDBUF,
    	(void *)&size, &reqsize) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"getsockopt SO_SNDBUF",ierr);
    }
#ifdef DEBUG
    cerr << "SocketImpl::getSendBufferSize()=" << size << endl;
#endif
    return size;
}

void SocketImpl::setTcpNoDelay(bool val) throw(IOException)
{
    int opt = val ? 1 : 0;
    socklen_t len = sizeof(opt);
    if (setsockopt(fd,SOL_TCP,TCP_NODELAY,(char *)&opt,len) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"setsockopt TCP_NODELAY",ierr);
    }
}

bool SocketImpl::getTcpNoDelay() throw(IOException)
{
    int opt = 0;
    socklen_t len = sizeof(opt);
    if (getsockopt(fd,SOL_TCP,TCP_NODELAY,(char *)&opt,&len) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"setsockopt TCP_NODELAY",ierr);
    }
    return opt != 0;
}

void SocketImpl::setKeepAlive(bool val) throw(IOException)
{
    if (getDomain() != PF_INET) return;
    int opt = val ? 1 : 0;
    socklen_t len = sizeof(opt);
    // man 7 socket
    if (setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,(char *)&opt,len) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"setsockopt SO_KEEPALIVE",ierr);
    }
}

bool SocketImpl::getKeepAlive() const throw(IOException)
{
    if (getDomain() != PF_INET) return false;
    int opt = 0;
    socklen_t len = sizeof(opt);
    if (getsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,(char *)&opt,&len) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"setsockopt TCP_NODELAY",ierr);
    }
    return opt != 0;
}

void SocketImpl::setKeepAliveIdleSecs(int val) throw(IOException)
{
    if (getDomain() != PF_INET) return;
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
    if (setsockopt(fd,SOL_TCP,TCP_KEEPIDLE,(char *)&val,len) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"setsockopt TCP_KEEPIDLE",ierr);
    }
}

int SocketImpl::getKeepAliveIdleSecs() const throw(IOException)
{
    if (getDomain() != PF_INET) return 0;
    int val = 0;
    socklen_t len = sizeof(val);
    if (getsockopt(fd,SOL_TCP,TCP_KEEPIDLE,(char *)&val,&len) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"setsockopt TCP_KEEPIDLE",ierr);
    }
    return val;
}

int SocketImpl::getInQueueSize() const throw(IOException)
{
    if (getDomain() != PF_INET) return 0;
    int val = 0;
    if (::ioctl(fd,SIOCINQ,&val) < 0) {	// man 7 tcp
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"getInQueueSize: ioctl SIOCINQ",ierr);
    }
    return val;
}

int SocketImpl::getOutQueueSize() const throw(IOException)
{
    if (getDomain() != PF_INET) return 0;
    int val = 0;
    if (::ioctl(fd,SIOCOUTQ,&val) < 0) {	// man 7 tcp
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),"getOutQueueSize: ioctl SIOCOUTQ",ierr);
    }
    return val;
}

#define JOIN_ALL_INTERFACES
#ifdef JOIN_ALL_INTERFACES
void SocketImpl::joinGroup(Inet4Address groupAddr) throw(IOException)
{
    list<Inet4NetworkInterface> ifcs = getInterfaces();
    // join on all interfaces
    for (list<Inet4NetworkInterface>::const_iterator ii = ifcs.begin();
        ii != ifcs.end(); ++ii) {
        Inet4NetworkInterface ifc = *ii;
        if (ifc.getFlags() & IFF_MULTICAST || ifc.getFlags() & IFF_LOOPBACK)
            joinGroup(groupAddr,ifc);
    }
}
#else
void SocketImpl::joinGroup(Inet4Address groupAddr) throw(IOException)
{
    struct ip_mreqn mreq;
    mreq.imr_multiaddr = groupAddr.getInAddr();
    mreq.imr_address.s_addr = INADDR_ANY;
    mreq.imr_ifindex = 0;

    if (::setsockopt(fd, SOL_IP, IP_ADD_MEMBERSHIP,
    	(void *)&mreq, sizeof(mreq)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(groupAddr.getHostAddress(),
	    "setsockopt IP_ADD_MEMBERSHIP",ierr);
    }
}

#endif

void SocketImpl::joinGroup(Inet4Address groupAddr,const Inet4NetworkInterface & iface) throw(IOException)
{
    struct ip_mreqn mreq;
    mreq.imr_multiaddr = groupAddr.getInAddr();
    mreq.imr_address = iface.getAddress().getInAddr();
    mreq.imr_ifindex = iface.getIndex();
#ifdef DEBUG
    cerr << "joining group, maddr=" << groupAddr.getHostAddress() <<
        " iface=" << iface.getAddress().getHostAddress() << 
        " index=" << iface.getIndex() << " iface name=" << iface.getName() << endl;
#endif

    if (::setsockopt(fd, SOL_IP, IP_ADD_MEMBERSHIP,
    	(void *)&mreq, sizeof(mreq)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(groupAddr.getHostAddress(),
	    "setsockopt IP_ADD_MEMBERSHIP",ierr);
    }
}

void SocketImpl::leaveGroup(Inet4Address groupAddr) throw(IOException)
{
    list<Inet4NetworkInterface> ifcs = getInterfaces();
    // leave on all interfaces
    for (list<Inet4NetworkInterface>::const_iterator ii = ifcs.begin(); ii != ifcs.end(); ++ii)
        leaveGroup(groupAddr,*ii);
}

void SocketImpl::leaveGroup(Inet4Address groupAddr,const Inet4NetworkInterface& iface)
	throw(IOException)
{
    struct ip_mreqn mreq;
    mreq.imr_multiaddr = groupAddr.getInAddr();
    mreq.imr_address = iface.getAddress().getInAddr();
    mreq.imr_ifindex = iface.getIndex();

    if (::setsockopt(fd, SOL_IP, IP_DROP_MEMBERSHIP,
    	(void *)&mreq, sizeof(mreq)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(groupAddr.getHostAddress(),
	    "setsockopt IP_DROP_MEMBERSHIP",ierr);
    }
}

void SocketImpl::setInterface(Inet4Address maddr,const Inet4NetworkInterface& iface) throw(IOException)
{
    struct ip_mreqn mreq;
    mreq.imr_multiaddr = maddr.getInAddr();
    mreq.imr_address = iface.getAddress().getInAddr();
    mreq.imr_ifindex = iface.getIndex();
#ifdef DEBUG
    cerr << "setInterface, maddr=" << maddr.getHostAddress() <<
        " iface=" << iface.getAddress().getHostAddress() << 
        " index=" << iface.getIndex() << " iface name=" << iface.getName() << endl;
#endif

    if (::setsockopt(fd, SOL_IP, IP_MULTICAST_IF,
    	(void *)&mreq, sizeof(mreq)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),
		"setsockopt IP_MULTICAST_IF",ierr);
    }
}

void SocketImpl::setInterface(Inet4Address maddr) throw(IOException)
{
    struct ip_mreqn mreq;
    mreq.imr_multiaddr = maddr.getInAddr();
    mreq.imr_address.s_addr = INADDR_ANY;
    mreq.imr_ifindex = 0;

    if (::setsockopt(fd, SOL_IP, IP_MULTICAST_IF,
    	(void *)&mreq, sizeof(mreq)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),
		"setsockopt IP_MULTICAST_IF",ierr);
    }
}

Inet4NetworkInterface SocketImpl::findInterface(const Inet4Address& iaddr) const
	throw(IOException)
{
    list<Inet4NetworkInterface> ifcs = getInterfaces();
    Inet4NetworkInterface mtch;
    int bits = 0;
    for (list<Inet4NetworkInterface>::const_iterator ii = ifcs.begin(); ii != ifcs.end(); ++ii) {
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
    struct ip_mreqn mreq;
    socklen_t reqsize = sizeof(mreq);
    memset(&mreq,0,reqsize);
    if (::getsockopt(fd, SOL_IP, IP_MULTICAST_IF,(void *)&mreq, &reqsize) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),
	    "getsockopt IP_MULTICAST_IF",ierr);
    }
#ifdef DEBUG
    cerr << "reqsize=" << reqsize << endl;
#endif
    // If the returned size is 4, then the contents of imr_multiaddr appears
    // to be the local interface address.
    if (reqsize == 4) {

#ifdef DEBUG
        cerr << "Inet4NetworkInterface SocketImpl::getInterface(): iface index=" <<
            mreq.imr_ifindex << 
        " mcastaddr = " << Inet4Address(ntohl(mreq.imr_multiaddr.s_addr)).getHostAddress() << 
        " address = " << Inet4Address(ntohl(mreq.imr_address.s_addr)).getHostAddress() <<  endl;
#endif
        Inet4Address addr(ntohl(mreq.imr_multiaddr.s_addr));
        list<Inet4NetworkInterface> ifaces = getInterfaces();
        list<Inet4NetworkInterface>::const_iterator ii = ifaces.begin();
        for ( ; ii != ifaces.end(); ++ii) {
            Inet4NetworkInterface iface = *ii;
            if (iface.getAddress() == addr) return iface;
        }
        return Inet4NetworkInterface("",addr,Inet4Address(INADDR_ANY),Inet4Address(INADDR_BROADCAST),0,0,0);
    }
#ifdef DEBUG
    cerr << "Inet4NetworkInterface SocketImpl::getInterface(): iface index=" <<
        mreq.imr_ifindex << 
        " mcastaddr = " << Inet4Address(ntohl(mreq.imr_multiaddr.s_addr)).getHostAddress() << 
        " address = " << Inet4Address(ntohl(mreq.imr_address.s_addr)).getHostAddress() <<  endl;
#endif

    if (mreq.imr_ifindex == 0)
        return Inet4NetworkInterface();

    struct ifreq ifreq;

    ifreq.ifr_ifindex = mreq.imr_ifindex;
    if (ioctl(getFd(),SIOCGIFNAME,&ifreq) < 0)
		throw IOException("Socket","ioctl(,SIOCGIFNAME,)",errno);
#ifdef DEBUG
    cerr << "Inet4NetworkInterface SocketImpl::getInterface(): iface name=" << ifreq.ifr_name << endl;
#endif

    return getInterface(ifreq.ifr_name);
}

void SocketImpl::setTimeToLive(int val) throw(IOException)
{
    if (setsockopt(fd, SOL_IP, IP_MULTICAST_TTL,
    	(void *)&val, sizeof(val)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),
	    "setsockopt IP_MULTICAST_TTL",ierr);
    }
}

int SocketImpl::getTimeToLive() const throw(IOException)
{
    int val;
    socklen_t reqsize = sizeof(val);
    if (::getsockopt(fd, SOL_IP, IP_MULTICAST_TTL,
    	(void *)&val, &reqsize) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),
		"getsockopt IP_MULTICAST_TTL",ierr);
    }
    return val;
}

void SocketImpl::setMulticastLoop(bool val) throw(IOException)
{
    int loop = (val ? 1 : 0);
    if (setsockopt(fd, SOL_IP, IP_MULTICAST_LOOP,
        (void*)&loop,sizeof(loop)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),
	    "setsockopt IP_MULTICAST_LOOP",ierr);
    }
}

bool SocketImpl::getMulticastLoop() const throw(IOException)
{
    int val;
    socklen_t reqsize = sizeof(val);
    if (::getsockopt(fd, SOL_IP, IP_MULTICAST_LOOP,
    	(void *)&val, &reqsize) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),
		"getsockopt IP_MULTICAST_LOOP",ierr);
    }
    return val != 0;
}

void SocketImpl::setBroadcastEnable(bool val) throw(IOException)
{
    int bcast = val ? 1 : 0;
    if (::setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
    	(void *)&bcast, sizeof(bcast)) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),
	    "setsockopt SO_BROADCAST",ierr);
    }
}

bool SocketImpl::getBroadcastEnable() const throw(IOException)
{
    int bcast;
    socklen_t slen = sizeof(bcast);
    if (::getsockopt(fd, SOL_SOCKET, SO_BROADCAST,
    	(void *)&bcast, &slen) < 0) {
	int ierr = errno;	// Inet4SocketAddress::toString changes errno
	throw IOException(localaddr->toString(),
	    "getsockopt SO_BROADCAST",ierr);
    }
    return bcast != 0;
}

Inet4NetworkInterface SocketImpl::getInterface(const string& name) const throw(IOException)
{
    struct ifreq ifreq;
    strcpy(ifreq.ifr_name,name.c_str());

    // ioctls in /usr/include/bits/ioctls.h
    if (ioctl(getFd(),SIOCGIFADDR,&ifreq) < 0)
            throw IOException("Socket","ioctl(,SIOCGIFADDR,)",errno);
    Inet4SocketAddress saddr = Inet4SocketAddress((const sockaddr_in*)&ifreq.ifr_addr);
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

#ifdef DEBUG
    cerr << "addr="  << saddr.toString() << endl;
#endif
    return Inet4NetworkInterface(name,addr,baddr,maddr,mtu,index,flags);
}

list<Inet4NetworkInterface> SocketImpl::getInterfaces() const throw(IOException)
{
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
#ifdef DEBUG
	cerr << "bufsize=" << bufsize << " is not enough" << endl;
#endif
    }
    assert((ifcs.ifc_len % sizeof(struct ifreq)) == 0);
    int ninterfaces = ifcs.ifc_len / sizeof(struct ifreq);
#ifdef DEBUG
    cerr << "ninterfaces=" << ninterfaces << endl;
#endif

    list<Inet4NetworkInterface> interfaces;
									    
    for (int i = 0; i < ninterfaces; i++) {
#ifdef DEBUG
	cerr << "name="  << ifcs.ifc_req[i].ifr_name << endl;
#endif
        Inet4NetworkInterface ifc = getInterface(ifcs.ifc_req[i].ifr_name);
        interfaces.push_back(ifc);

#ifdef DEBUG
	cerr << "addr="  << saddr.toString() << endl;
#endif
    }
    return interfaces;
}

Socket::Socket(int domain) throw(IOException) :
	impl(domain,SOCK_STREAM)
{
}

Socket::Socket(int fd, const SocketAddress& remoteaddr) throw(IOException) :
	impl(fd,remoteaddr)
{
}

Socket::Socket(const std::string& dest,int port)
	throw(UnknownHostException,IOException) :
	impl(PF_INET,SOCK_STREAM)
{
    connect(dest,port);
}

Socket::Socket(const Inet4Address& addr,int port)
	throw(IOException) :
	impl(PF_INET,SOCK_STREAM)
{
    connect(addr,port);
}

Socket::Socket(const SocketAddress& addr)
	throw(IOException) :
	impl(addr.getFamily(),SOCK_STREAM)
{
    connect(addr);
}

ServerSocket::ServerSocket(int port,int backlog)
	throw(IOException) :
	impl(PF_INET,SOCK_STREAM)
{
    impl.setBacklog(backlog);
    impl.bind(port);
}

ServerSocket::ServerSocket(const Inet4Address& addr, int port,int backlog)
	throw(IOException) :
	impl(PF_INET,SOCK_STREAM)
{
    impl.setBacklog(backlog);
    impl.bind(addr,port);
}

ServerSocket::ServerSocket(const SocketAddress& addr,int backlog)
	throw(IOException) :
	impl(addr.getFamily(),SOCK_STREAM)
{
    impl.setBacklog(backlog);
    impl.bind(addr);
}

DatagramSocket::DatagramSocket() throw(IOException) :
	impl(PF_INET,SOCK_DGRAM)
{
}

DatagramSocket::DatagramSocket(int port) throw(IOException) :
	impl(PF_INET,SOCK_DGRAM)
{
    impl.bind(port);
}

DatagramSocket::DatagramSocket(const Inet4Address& addr,int port)
	throw(IOException) :
	impl(PF_INET,SOCK_DGRAM)
{
    impl.bind(addr,port);
}

DatagramSocket::DatagramSocket(const SocketAddress& addr)
	throw(IOException) :
	impl(addr.getFamily(),SOCK_DGRAM)
{
    impl.bind(addr);
}

/* static */
vector<Socket*> Socket::createSocketPair(int type) throw(IOException)
{
    int fds[2];
    if (::socketpair(PF_UNIX,type,0,fds) < 0) 
    	throw IOException("socketpair","create",errno);
    vector<Socket*> res;
    res.push_back(new Socket(fds[0],UnixSocketAddress("")));
    res.push_back(new Socket(fds[1],UnixSocketAddress("")));
    return res;
}
