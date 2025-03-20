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

#include "McSocket.h"
#include "Logger.h"
#include "UTime.h"       // MSECS_PER_SEC
#include "auto_ptr.h"
#include "IOTimeoutException.h"

#include <iostream>

using namespace nidas::util;
using namespace std;


namespace
{
    typedef map<Inet4SocketAddress,McSocketListener*> listener_map_t;
    Mutex listener_mutex;
    listener_map_t listener_map;
}


/* static */
const int McSocketDatagram::magicVal = 0x01234567;

McSocketDatagram::McSocketDatagram(int requestType) :
    DatagramPacketT<McSocketData>(&mcdata,1),mcdata()
{
    setRequestType(requestType);
}

/*
 * Copy constructor.
 */
McSocketDatagram::McSocketDatagram(const McSocketDatagram& x):
    DatagramPacketT<McSocketData>(x),mcdata(x.mcdata)
{
    data = &mcdata; // point to new mcdata
}

/*
 * Assignment operator.
 */
McSocketDatagram& McSocketDatagram::operator=(const McSocketDatagram& rhs)
{
    if (this != &rhs) {
        // invoke subclass assignment.
        (*(DatagramPacketT<McSocketData>*)this) =
                (DatagramPacketT<McSocketData>) rhs;
        mcdata = rhs.mcdata;    // copy the data
        data = &mcdata; // point to new mcdata
    }
    return *this;
}

/* static */
void McSocketListener::accept(McSocket<Socket>* mcsocket)
{
    const Inet4SocketAddress& mcAddr = mcsocket->getInet4McastSocketAddress();

    VLOG(("McSocketListener::accept, listener_map.size()=")
         << listener_map.size() << " mcAddr=" << mcAddr.toAddressString()
         << " request=" << mcsocket->getRequestType() << " socketType="
         << getMcSocketType(mcsocket));

    listener_mutex.lock();
    McSocketListener* lnr = listener_map[mcAddr];

    if (!lnr) {
        lnr = new McSocketListener(mcAddr);
        listener_map[mcAddr] = lnr;
        listener_mutex.unlock();

        lnr->add(mcsocket);
        lnr->start();
    }
    else {
        listener_mutex.unlock();
        lnr->add(mcsocket);
    }
}

/* static */
void McSocketListener::accept(McSocket<DatagramSocket>* mcsocket)
{
    const Inet4SocketAddress& mcAddr = mcsocket->getInet4McastSocketAddress();

    VLOG(("McSocketListener::accept, listener_map.size()=")
         << listener_map.size() << " mcAddr=" << mcAddr.toAddressString()
         << " request=" << mcsocket->getRequestType() << " socketType="
         << getMcSocketType(mcsocket));

    listener_mutex.lock();
    McSocketListener* lnr = listener_map[mcAddr];

    if (!lnr) {
        lnr = new McSocketListener(mcAddr);
        listener_map[mcAddr] = lnr;
        listener_mutex.unlock();

        lnr->add(mcsocket);
        lnr->start();
    }
    else {
        listener_mutex.unlock();
        lnr->add(mcsocket);
    }
}

/* static */
void McSocketListener::close(McSocket<Socket>* mcsocket)
{
    const Inet4SocketAddress& mcAddr = mcsocket->getInet4McastSocketAddress();

    Synchronized sync(listener_mutex);

    VLOG(("McSocketListener::close, map size=") << listener_map.size());
    map<Inet4SocketAddress,McSocketListener*>::iterator mapi =
        listener_map.find(mcAddr);

    if (mapi == listener_map.end()) return;

    McSocketListener* lnr = mapi->second;
    if (!lnr) return;

    // how many McSockets are still being serviced
    int nsock = lnr->remove(mcsocket);
    VLOG(("nsock=") << nsock);
    if (nsock == 0) {
        lnr->interrupt();
        // lnr->cancel();
        VLOG(("McSocketListener::close, lnr->join()"));
        lnr->join();
        VLOG(("McSocketListener::close, lnr->joined"));
        delete lnr;
        listener_map.erase(mapi);
    }
}

/* static */
void McSocketListener::close(McSocket<DatagramSocket>* mcsocket)
{
    const Inet4SocketAddress& mcAddr = mcsocket->getInet4McastSocketAddress();

    Synchronized sync(listener_mutex);

    VLOG(("McSocketListener::close, map size=") << listener_map.size());
    map<Inet4SocketAddress,McSocketListener*>::iterator mapi =
        listener_map.find(mcAddr);

    if (mapi == listener_map.end()) return;

    McSocketListener* lnr = mapi->second;
    if (!lnr) return;

    // how many McSockets are still being serviced
    int nsock = lnr->remove(mcsocket);
    VLOG(("nsock=") << nsock);
    if (nsock == 0) {
        lnr->interrupt();
        // lnr->cancel();
        VLOG(("McSocketListener::close, lnr->join()"));
        lnr->join();
        VLOG(("McSocketListener::close, lnr->joined"));
        delete lnr;
        listener_map.erase(mapi);
    }
}

/* static */
int McSocketListener::check() throw()
{
    Synchronized sync(listener_mutex);
    return listener_map.size();
}

McSocketListener::McSocketListener(const Inet4SocketAddress&
                                   mcastaddr) :
    Thread(string("McSocketListener: ") + mcastaddr.toAddressString()),
        _mcastAddr(mcastaddr),_mcsocket_mutex(),_readsock(0),_tcpMcSockets(),
        _udpMcSockets()
{
    // block SIGUSR1, then unblock it in pselect/ppoll
    blockSignal(SIGUSR1);
}

McSocketListener::~McSocketListener()
{
    if (_readsock) _readsock->close();
    delete _readsock;
}

void McSocketListener::add(McSocket<Socket>* mcsocket)
{
    VLOG(("McSocketListener add mcsocket=%p,requestType=%d",
          mcsocket,mcsocket->getRequestType()));
    _mcsocket_mutex.lock();
    if (_tcpMcSockets[mcsocket->getRequestType()])
        DLOG(("McSocketListener TCP requestType=%d already added",
              mcsocket->getRequestType()));
    else _tcpMcSockets[mcsocket->getRequestType()] = mcsocket;
    _mcsocket_mutex.unlock();
}

void McSocketListener::add(McSocket<DatagramSocket>* mcsocket)
{
    DLOG(("McSocketListener add requestType=%d", mcsocket->getRequestType()));
    _mcsocket_mutex.lock();
    if (_udpMcSockets[mcsocket->getRequestType()])
        DLOG(("McSocketListener UDP requestType=%d already added",
              mcsocket->getRequestType()));
    else
        _udpMcSockets[mcsocket->getRequestType()] = mcsocket;
    _mcsocket_mutex.unlock();
}

int McSocketListener::remove(McSocket<Socket>* mcsocket)
{
    VLOG(("McSocketListener remove mcsocket=%p, requestType=%d, size=%d",
          mcsocket,mcsocket->getRequestType(),_tcpMcSockets.size()));
    Synchronized autolock(_mcsocket_mutex);

    map<int,McSocket<Socket>*>::iterator mapi =
        _tcpMcSockets.find(mcsocket->getRequestType());

    // When a McSocket has established a socket connection
    // a copy of the original is usually made, and then eventually
    // this McSocketListener::remove will be attempted with the
    // copy.  The copy will not be found in _tcpMcSockets,
    // here, but that is OK.
    if (mapi != _tcpMcSockets.end() && mapi->second == mcsocket)
        _tcpMcSockets.erase(mapi);
    else
        VLOG(("") << "McSocketListener::remove mcsocket=" << mcsocket
             << " not found");

    int nsock = _tcpMcSockets.size();
    return nsock;
}

int McSocketListener::remove(McSocket<DatagramSocket>* mcsocket)
{
    VLOG(("McSocketListener remove requestType=%d",
          mcsocket->getRequestType()));
    Synchronized autolock(_mcsocket_mutex);

    map<int,McSocket<DatagramSocket>*>::iterator mapi =
        _udpMcSockets.find(mcsocket->getRequestType());

    if (mapi != _udpMcSockets.end() && mapi->second == mcsocket)
        _udpMcSockets.erase(mapi);

    int nsock = _udpMcSockets.size();
    return nsock;
}

void McSocketListener::interrupt()
{
    Thread::interrupt();
    try {
        kill(SIGUSR1);
    }
    catch(const Exception& e) {
        PLOG(("%s",e.what()));
    }
}

int McSocketListener::run()
{
    if (_mcastAddr.getInet4Address().isMultiCastAddress())
    {
        DLOG(("entering run(): ")
             << "listening for McSocket requests on multicast address "
             << _mcastAddr.toString());
        // can't bind to a specific address, must bind to INADDR_ANY.
        MulticastSocket* msock = new MulticastSocket(_mcastAddr.getPort());
        _readsock = msock;
        vector<Inet4NetworkInterface> interfaces;
        listMulticastInterfaces(msock, interfaces);
        vector<Inet4NetworkInterface>::const_iterator ifit;
        for (ifit = interfaces.begin(); ifit != interfaces.end(); ++ifit)
        {
            VLOG(("joining interface ") << ifit->getName());
            msock->joinGroup(_mcastAddr.getInet4Address(), *ifit);
        }
    }
    else
    {
        DLOG(("entering run(): ")
             << "listening for McSocket requests on udp address "
             << _mcastAddr.toString());
        _readsock = new DatagramSocket(_mcastAddr.getPort());
    }

    McSocketDatagram dgram;
    Inet4PacketInfoX pktinfo;

    // get the existing signal mask
    sigset_t sigmask;
    pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
    // unblock SIGUSR1 in pselect/ppoll
    sigdelset(&sigmask,SIGUSR1);

#ifdef HAVE_PPOLL
    struct pollfd fds;
    fds.fd =  _readsock->getFd();
#ifdef POLLRDHUP
    fds.events = POLLIN | POLLRDHUP;
#else
    fds.events = POLLIN;
#endif
#else
    fd_set readfds;
    FD_ZERO(&readfds);
#endif

    while (!isInterrupted()) {

        DLOG(("") << "waiting on " << _mcastAddr.toString() << "...");
#ifdef HAVE_PPOLL
        int nfd = ::ppoll(&fds,1,NULL,&sigmask);
        if (nfd < 0) {
            if (errno == EINTR) continue;
            throw nidas::util::IOException(_readsock->getLocalSocketAddress().toString(), "ppoll",errno);
        }
        if (fds.revents & POLLERR)
            throw nidas::util::IOException(_readsock->getLocalSocketAddress().toString(), "receive",errno);

#ifdef POLLRDHUP
        if (fds.revents & (POLLHUP | POLLRDHUP))
#else
            if (fds.revents & POLLHUP)
#endif
                WLOG(("%s POLLHUP",_readsock->getLocalSocketAddress().toString().c_str()));

        if (! (fds.revents & POLLIN)) continue;

#else
        int fd = _readsock->getFd();
        assert(fd >= 0 && fd < FD_SETSIZE);     // FD_SETSIZE=1024
        FD_SET(fd,&readfds);
        int nfd = ::pselect(fd+1,&readfds,NULL,NULL,0,&sigmask);
        if (nfd < 0) {
            if (errno == EINTR) continue;
            throw nidas::util::IOException(_readsock->getLocalSocketAddress().toString(), "pselect",errno);
        }
#endif

        _readsock->receive(dgram,pktinfo,0);

        ILOG(("McSocket request from %s, magic=0x%x, reqtype=%d, port=%d, "
              "socktype=%d, len=%d",
              dgram.getSocketAddress().toAddressString().c_str(),
              dgram.getMagic(),dgram.getRequestType(),
              dgram.getRequesterListenPort(),dgram.getSocketType(),
              dgram.getLength()));

        if (dgram.getLength() != sizeof(struct McSocketData)) continue;
        if (dgram.getMagic() != dgram.magicVal) continue;

        Inet4SocketAddress remoteAddr;
        if (dgram.getSocketAddress().getFamily() == AF_INET) {
            remoteAddr = Inet4SocketAddress((const struct sockaddr_in*)
                                            dgram.getSocketAddress().getConstSockAddrPtr());
        }

        // socket address to send replies to
        remoteAddr.setPort(dgram.getRequesterListenPort());

        pktinfo.setRemoteSocketAddress(remoteAddr);

        // look for an mcsocket matching this request
        switch (dgram.getSocketType()) {
        case SOCK_STREAM:
            {
                _mcsocket_mutex.lock();
                McSocket<Socket>* mcsocket = 0;
                map<int,McSocket<Socket>*>::iterator mapi =
                    _tcpMcSockets.find(dgram.getRequestType());
                if (mapi != _tcpMcSockets.end()) mcsocket = mapi->second;
                _mcsocket_mutex.unlock();

                if (!mcsocket) {
                    WLOG(("No TCP McSocket for request type %d from host %s",
                          dgram.getRequestType(),
                          dgram.getSocketAddress().toAddressString().c_str()));
                    continue;
                }
                // create and connect socket to remoteAddr
                Socket* remote = 0;
                try {
                    remote = new Socket();
                    remote->connect(remoteAddr);
                    mcsocket->offer(remote,pktinfo);
                }
                catch (const IOException& ioe) {
                    ELOG(("McSocketListener: error connecting socket to %s: %s",
                          remoteAddr.toAddressString().c_str(),ioe.what()));
                    if (remote) remote->close();
                    delete remote;
                    mcsocket->offer(ioe.getErrno());
                }
            }
            break;
        case SOCK_DGRAM:
            {
                _mcsocket_mutex.lock();
                McSocket<DatagramSocket>* mcsocket = 0;
                map<int,McSocket<DatagramSocket>*>::iterator mapi =
                    _udpMcSockets.find(dgram.getRequestType());
                if (mapi != _udpMcSockets.end()) mcsocket = mapi->second;
                _mcsocket_mutex.unlock();

                if (!mcsocket) {
                    WLOG(("No UDP McSocket for request type %d from host %s",
                          dgram.getRequestType(),
                          dgram.getSocketAddress().toAddressString().c_str()));
                    continue;
                }
                DatagramSocket* remote = 0;
                try {
                    remote = new DatagramSocket();
                    remote->connect(remoteAddr);
                    mcsocket->offer(remote,pktinfo);
                }
                catch (const IOException& ioe) {
                    ELOG(("McSocketListener: error connecting socket to %s: %s",
                          remoteAddr.toAddressString().c_str(),ioe.what()));
                    if (remote) remote->close();
                    delete remote;
                    mcsocket->offer(ioe.getErrno());
                }
            }
            break;
        default:
            ELOG(("unknown data socket type"));
            break;
        }
    }
    VLOG(("McSocketListener::run returning"));
    _readsock->close();
    return 0;
}


void
nidas::util::
listMulticastInterfaces(MulticastSocket* requestmsock,
                        std::vector<Inet4NetworkInterface>& ifaces)
{
    ifaces.clear();
    VLOG(("INADDR_ANY: looking for multicast interfaces..."));
    std::list<Inet4NetworkInterface> tmpifaces =
        requestmsock->getInterfaces();
    std::list<Inet4NetworkInterface>::const_iterator ifacei =
        tmpifaces.begin();
    for ( ; ifacei != tmpifaces.end(); ++ifacei)
    {
        Inet4NetworkInterface iface = *ifacei;
        int flags = iface.getFlags();
        if ((flags & IFF_UP) &&
            (flags & IFF_BROADCAST) &&
            (flags & (IFF_MULTICAST | IFF_LOOPBACK)))
        {
            VLOG(("...found active multicast interface: ")
                 << iface.getName()
                 << "[" << iface.getIndex() << "]: "
                 << iface.getAddress().getHostAddress());
            ifaces.push_back(iface);
        }
    }
    VLOG(("") << "found " << ifaces.size() << " interfaces.");
}
